# THE VOC

Plugin **VST3** (Windows e macOS) de harmonizador vocal: gera até 4 vozes harmônicas a partir de uma voz
(ou instrumento monofônico), com coro, humanização e controle por MIDI. Visual minimalista anos 80.

## Funções
- **Harmony Source** (7 modos): Fixed Interval, Scale Interval, Chord Name, Chord Degrees, Chord Via MIDI, MIDI Omni, MIDI Channels
- 4 vozes com Interval, Gain, Pan, Mute/Solo, **Throat Length** (formante) e vibrato próprio (rate, onset, pitch, amp)
- Entrada: Input Range, Tracking, voz original com gain/pan/mute/solo
- Spread, Register, Inversion, MIDI Velocity Sensitivity
- Humanize: Naturalize, Pitch Variation, Timing Variation · Glide · Freeze
- Coro 2/4/8/16 vozes por voz, com variação de vibrato/pitch/timing e stereo spread
- Todos os parâmetros são automatizáveis na DAW (use a automação para trocar acordes ao longo da música)
- Latência fixa de ~70 ms, informada ao host (compensação automática)

## Compilar (automático)
Cada `git push` na branch `main` compila Windows e macOS no GitHub Actions.
Baixe o plugin em **Actions → Build THE VOC → Artifacts**.
Para publicar uma Release com os `.zip`: `git tag v0.1.0 && git push origin v0.1.0`.

## Compilar localmente
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
Requer CMake 3.22+, Visual Studio 2022 (Windows) ou Xcode (macOS). O JUCE é baixado automaticamente.

## Instalar
- **Windows:** copie a pasta `THE VOC.vst3` para `C:\Program Files\Common Files\VST3`
- **macOS:** copie `THE VOC.vst3` para `/Library/Audio/Plug-Ins/VST3` e rode `xattr -cr "/Library/Audio/Plug-Ins/VST3/THE VOC.vst3"` (plugin sem assinatura da Apple)

## Teste do motor
`tests/engine_test.cpp` roda sem JUCE e confere se as vozes saem nas notas corretas:
`c++ -O2 -std=c++17 -o engine_test tests/engine_test.cpp && ./engine_test`

## Licença
O JUCE é AGPLv3 ou licença comercial. Para distribuir o THE VOC, publique o código sob AGPLv3 (como este repositório)
ou adquira uma licença JUCE.
