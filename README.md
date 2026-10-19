# PolarisVCEOff

Plugin do [Lilu](https://github.com/acidanthera/Lilu) que desliga o codificador
H.264 por hardware (VCE) das placas AMD Polaris no macOS. Com ele, o VCE nunca
recebe trabalho e, por isso, não trava. O HEVC continua por hardware.

## O problema

Na RX 560 (Polaris 21, `1002:67FF`), o codificador H.264 da placa, o VCE 3.4,
para de responder no macOS. A parada acontece ao acaso, em média uma vez a cada
~14.500 quadros: uns 4 minutos gravando a tela a 2560x1080 e 60 fps.

Quando o VCE trava:

- nenhum reinício do driver da Apple faz ele voltar;
- a recuperação prende o WindowServer, e a tela congela;
- ~2 minutos depois, o watchdog reinicia o Mac (`userspace watchdog timeout:
  no successful checkins from WindowServer`).

Qualquer programa que grave ou codifique em H.264 por hardware dispara o
problema: o Cmd+Shift+5, o QuickTime, o OBS com o codificador de hardware, o
FaceTime e outros.

Nos testes, mudar o clock da memória, manter o VCE sempre ligado, limitar a uma
sessão e mexer na recuperação do driver não mudaram a frequência dos
travamentos. No Linux, a mesma placa codificou 54.000 quadros sem falha. Por
isso, o defeito é do caminho do driver da Apple com esta placa, e não da placa.

## O que o kext faz

Ele tem um único gancho, em `AMDRadeonX4000_AMDVCEHWEngine::requestCapability`,
a função que reserva o VCE para uma sessão de codificação. O gancho sempre
recusa a reserva e não chama o driver original, então o VCE nunca é usado.

Ele não escreve na memória do driver, não muda clock nem energia e não mexe em
outro bloco da GPU.

O resultado:

| | Sem o kext | Com o kext |
| --- | --- | --- |
| H.264 por hardware | trava ao acaso e reinicia o Mac | recusado; o programa usa o H.264 por software ou mostra erro |
| HEVC (H.265) por hardware | funciona | funciona: usa outro bloco da placa (UVD ENC), que o kext não toca |
| Decodificação de vídeo | funciona | funciona |

A cada sessão recusada, o kext grava no log do sistema:

```text
PolarisVCEOff: hardware H.264 encoder (VCE) is disabled on this GPU; session N refused, use software H.264 or HEVC
```

## Onde foi testado

| | |
| --- | --- |
| Placa | AMD Radeon RX 560 (Polaris 21, `1002:67FF`) |
| macOS | 26.7.1 Tahoe (25G241) |
| Driver | `com.apple.kext.AMDRadeonX4000` 7.0.1 |
| Bootloader | OpenCore 1.0.8 |
| Kexts | Lilu 1.7.2, WhateverGreen 1.7.1 |

O kext deve servir para outras placas Polaris (RX 460/470/480/550/570/580/590),
que usam o mesmo driver e o mesmo bloco VCE, mas ele não foi testado nelas.

## Requisitos

- Um Mac Intel ou Hackintosh com placa AMD Polaris e o macOS do Sierra ao
  Tahoe.
- [Lilu](https://github.com/acidanthera/Lilu/releases) 1.4.0 ou mais novo
  carregado. Use a versão mais nova para o seu macOS.
- Para compilar: as ferramentas de linha de comando do Xcode
  (`xcode-select --install`) e o código-fonte do Lilu.

## Compilar

1. Baixe o código-fonte do Lilu ao lado desta pasta, com o nome `lilu-src`:

   ```sh
   git clone https://github.com/acidanthera/Lilu.git lilu-src
   ```

   O kext usa os cabeçalhos do Lilu e o `Library/plugin_start.cpp` dele. Não
   é preciso compilar o Lilu.

2. Compile:

   ```sh
   ./build.sh
   ```

   Se o Lilu estiver em outro lugar, passe o caminho da pasta `Lilu` de dentro
   do repositório dele:

   ```sh
   LILU_DIR=/caminho/para/lilu-src/Lilu ./build.sh
   ```

O kext sai em `build/PolarisVCEOff.kext`.

## Instalar (OpenCore)

1. Copie `build/PolarisVCEOff.kext` para `EFI/OC/Kexts/`.
2. No `config.plist`, acrescente uma entrada em `Kernel` → `Add`, **depois da
   do Lilu**:

   ```xml
   <dict>
       <key>Arch</key>
       <string>x86_64</string>
       <key>BundlePath</key>
       <string>PolarisVCEOff.kext</string>
       <key>Comment</key>
       <string>PolarisVCEOff: H.264 VCE disabled</string>
       <key>Enabled</key>
       <true/>
       <key>ExecutablePath</key>
       <string>Contents/MacOS/PolarisVCEOff</string>
       <key>MaxKernel</key>
       <string></string>
       <key>MinKernel</key>
       <string></string>
       <key>PlistPath</key>
       <string>Contents/Info.plist</string>
   </dict>
   ```

3. Confira o `config.plist` com o `ocvalidate` da sua versão do OpenCore e
   reinicie.

Não precisa de nenhum boot-arg. Teste primeiro num pendrive com uma cópia da sua
EFI, antes de mudar a EFI do disco.

## Conferir se está funcionando

1. O kext carregou e o gancho foi aplicado:

   ```sh
   ioreg -p IOService -l -n PolarisVCEOff | grep -E 'Hook_Applied|H264_Hardware_Encoder|VCE_Sessions_Rejected'
   ```

   O resultado esperado:

   ```text
   "Hook_Applied" = Yes
   "H264_Hardware_Encoder" = "Disabled"
   "VCE_Sessions_Rejected" = 0
   ```

   `VCE_Sessions_Rejected` conta as sessões H.264 recusadas desde a partida.

2. O H.264 por hardware é recusado (precisa do `ffmpeg`, por exemplo do
   Homebrew):

   ```sh
   ffmpeg -f lavfi -i testsrc=size=1280x720:rate=30 -t 3 -c:v h264_videotoolbox -f null -
   ```

   Tem que falhar com `Cannot create compression session: -12903`, e o
   `VCE_Sessions_Rejected` tem que subir. Com `-allow_sw 1`, o mesmo comando
   funciona, codificando por software.

3. O HEVC por hardware funciona:

   ```sh
   ffmpeg -f lavfi -i testsrc=size=1280x720:rate=30 -t 3 -c:v hevc_videotoolbox -f null -
   ```

4. As mensagens do kext no log:

   ```sh
   /usr/bin/log show --last 10m --predicate 'eventMessage CONTAINS "PolarisVCEOff"'
   ```

## Gravar a tela

- **Para gravar por hardware, use HEVC.** No OBS, escolha o codificador
  "Apple VT HEVC Hardware Encoder".
- **O Cmd+Shift+5 e o QuickTime** gravam em H.264. Com o kext, eles passam
  sozinhos para o codificador por software e o vídeo sai normal, mas o software
  é lento em resolução alta: ~20 quadros por segundo a 2560x1080 num Core
  i5-11400.
- **Programas que exigem H.264 por hardware** mostram erro ao começar: por
  exemplo, o OBS com "Apple VT H264 Hardware Encoder" ou o `ffmpeg` sem
  `-allow_sw 1`. Troque para HEVC ou para um codificador por software, como o
  x264.

## Depois de atualizar o macOS

Repita o "Conferir se está funcionando". O gancho procura a função pelo nome
completo, com os tipos dos argumentos. Por isso:

- **Se a Apple renomear a função ou mudar os argumentos,** o gancho não é
  aplicado (`Hook_Applied = No`). O H.264 volta a usar o VCE e volta a travar,
  mas o kext não causa pânico. O log mostra
  `ERROR: failed to route requestCapability`.
- **Se a Apple mudar só o tipo de retorno,** o valor que o kext devolve pode
  mudar de sentido. Isso se percebe no passo 2: o `ffmpeg` deixaria de falhar.
- **Numa versão do macOS depois do Tahoe,** o Lilu não carrega o kext, porque
  ele está marcado como compatível até o Tahoe.

Em qualquer desses casos, não use H.264 por hardware até o kext ser ajustado.
Mantenha o Lilu e o WhateverGreen atualizados para a versão do macOS.

## Desligar

- **Numa partida só,** acrescente `-polarisvceoffoff` aos boot-args. O Lilu
  não carrega o kext, e o H.264 volta para o VCE, com o risco de travar.
- **De vez,** ponha `Enabled` = `false` na entrada do `config.plist` ou tire o
  kext da EFI.

Também existem `-polarisvceoffdbg` (registro de depuração do Lilu) e
`-polarisvceoffbeta` (carrega numa versão do macOS ainda não marcada como
compatível).

## Arquivos

| Arquivo | O que é |
| --- | --- |
| `PolarisVCEOff.cpp` | o kext: o gancho e as propriedades do `ioreg` |
| `kmod_info.c` | a identificação do kext para o kernel |
| `Info.plist` | o pacote do kext e a dependência do Lilu |
| `build.sh` | compila e monta `build/PolarisVCEOff.kext` |

O código do Lilu não faz parte deste repositório. Ele é baixado à parte e tem a
licença dele.
