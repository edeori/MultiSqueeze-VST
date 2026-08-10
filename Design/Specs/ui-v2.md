# MultiSqueeze UI v2

The v2 direction keeps the current Moth Production identity, but shifts it from a mostly textured skin toward a precision dynamics instrument. The distinctive elements are the segmented band rail, dedicated metering, restrained colored band fields and the machined knob system.

## Reference

- Canvas: `1180 × 720`
- Mockup: `Design/Mockups/multisqueeze-ui-v2-1180x720.png`
- High-resolution concept source: `Design/Mockups/multisqueeze-ui-v2-source.png`
- Implementation assets: `Assets/UI-v2`

## Layout map

| Region | Reference bounds | Purpose |
| --- | --- | --- |
| Header | `0, 0, 1180, 52` | maker, centered moth mark, global bypass/menu |
| Inspector | `14, 60, 272, 574` | selected band, detector, states, GR meter, add/remove |
| Main workspace | `295, 60, 871, 574` | band rail, spectrum, I/O meters and five controls |
| Spectrum plot | `322, 138, 734, 252` | FFT, crossover and per-band selection |
| I/O meter lane | `1066, 138, 82, 252` | compact input/output peak meters |
| Rotary row | `295, 420, 871, 214` | threshold, ratio, attack, release, makeup |
| Master rail | `4, 642, 1172, 74` | output slider and value |

All bounds are reference values, not fixed pixel requirements. Scale from the `1180 × 720` design space and preserve the inspector width ratio until the existing minimum window size forces a compact layout.

## Design tokens

| Token | Value | Use |
| --- | --- | --- |
| `canvas` | `#07090B` | deepest wells |
| `surface-1` | `#0D1115` | primary panels |
| `surface-2` | `#141A20` | controls and raised states |
| `surface-3` | `#1B232A` | hover/secondary fill |
| `stroke` | `#28323B` | hairline borders and inactive ticks |
| `text` | `#EAF2F5` | primary labels and values |
| `text-muted` | `#84919A` | scales and helper text |
| `cyan` | `#36D7FF` | selection, output trace, active controls |
| `lime` | `#D8F35A` | gain reduction only |
| `amber` | `#FFB547` | warning/limiter state only |

Band fills should be subtle and stay below `10%` opacity. Suggested hues: low/cyan, low-mid/teal, high-mid/amber, high/violet-gray. Crossover lines remain cyan so their interaction role is consistent.

## Type

- Product: `Oxanium Bold`
- Section labels and button labels: `Barlow Condensed Medium`
- Secondary captions: `Barlow Condensed Light`
- Numeric readouts and scales: `Rajdhani Medium`
- Use uppercase labels with tracking; never add manual spaces between letters when the font API can provide tracking.

## Asset strategy

`background-v2` owns only material, recesses, vents and chassis detail. The spectrum, meters, state colors, text, borders and all data-dependent graphics must be drawn in JUCE. This prevents blurred text, keeps states accessible and lets the interface scale cleanly.

`knob-v2` contains the physical cap and its ivory index only. Draw the cyan value arc outside the rotating image from the rotary start angle to the current value angle. Recommended cap size at the reference canvas is `98–108 px`; keep ticks outside the bitmap.

The SVG files are intentionally simple and monochrome. Parse them as `juce::Drawable` assets, or recreate their paths directly with `juce::Path` and recolor by component state.

## State rules

- Hover: raise stroke brightness, no glow bloom.
- Active/selected: cyan stroke plus a low-opacity cyan fill.
- Bypass: muted gray text and a diagonal/amber status cue; do not reuse the same appearance as active.
- Solo: cyan active state.
- Mute: amber active state.
- Gain reduction: lime segments grow downward from `0 dB`; hold peak for approximately `600 ms`.
- I/O meters: cyan until the final three segments, then amber; clip hold may use off-white.
- Keyboard focus: `2 px` cyan outline inside the component bounds.

## Implementation note

The generated mockup is a visual target, not a pixel-perfect source of truth for text. Preserve the parameter names and behavior already present in the code. The extra I/O meters and band overview rail are the two most valuable functional additions; the decorative venting and fasteners are already baked into the background plate.
