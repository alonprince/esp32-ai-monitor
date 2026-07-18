---
name: NEXUS-OS
colors:
  surface: '#131313'
  surface-dim: '#131313'
  surface-bright: '#393939'
  surface-container-lowest: '#0e0e0e'
  surface-container-low: '#1b1b1b'
  surface-container: '#1f1f1f'
  surface-container-high: '#2a2a2a'
  surface-container-highest: '#353535'
  on-surface: '#e2e2e2'
  on-surface-variant: '#e3bfb3'
  inverse-surface: '#e2e2e2'
  inverse-on-surface: '#303030'
  outline: '#aa897f'
  outline-variant: '#5b4138'
  surface-tint: '#ffb59c'
  primary: '#ffb59c'
  on-primary: '#5c1900'
  primary-container: '#ff5f1f'
  on-primary-container: '#561700'
  inverse-primary: '#ab3600'
  secondary: '#c8c6c5'
  on-secondary: '#313030'
  secondary-container: '#474746'
  on-secondary-container: '#b7b5b4'
  tertiary: '#c6c6c7'
  on-tertiary: '#2f3131'
  tertiary-container: '#939494'
  on-tertiary-container: '#2b2d2d'
  error: '#ffb4ab'
  on-error: '#690005'
  error-container: '#93000a'
  on-error-container: '#ffdad6'
  primary-fixed: '#ffdbcf'
  primary-fixed-dim: '#ffb59c'
  on-primary-fixed: '#390c00'
  on-primary-fixed-variant: '#832700'
  secondary-fixed: '#e5e2e1'
  secondary-fixed-dim: '#c8c6c5'
  on-secondary-fixed: '#1c1b1b'
  on-secondary-fixed-variant: '#474746'
  tertiary-fixed: '#e2e2e2'
  tertiary-fixed-dim: '#c6c6c7'
  on-tertiary-fixed: '#1a1c1c'
  on-tertiary-fixed-variant: '#454747'
  background: '#131313'
  on-background: '#e2e2e2'
  surface-variant: '#353535'
typography:
  clock-display:
    fontFamily: Inter
    fontSize: 72px
    fontWeight: '100'
    lineHeight: 72px
    letterSpacing: -0.02em
  headline-md:
    fontFamily: Inter
    fontSize: 24px
    fontWeight: '600'
    lineHeight: 28px
  body-sm:
    fontFamily: Geist
    fontSize: 14px
    fontWeight: '400'
    lineHeight: 20px
  label-technical:
    fontFamily: JetBrains Mono
    fontSize: 10px
    fontWeight: '700'
    lineHeight: 12px
    letterSpacing: 0.1em
  status-bar:
    fontFamily: JetBrains Mono
    fontSize: 12px
    fontWeight: '500'
    lineHeight: 14px
rounded:
  sm: 0.5rem
  DEFAULT: 1rem
  md: 1.5rem
  lg: 2rem
  xl: 3rem
  full: 9999px
spacing:
  screen-padding: 24px
  card-gap: 8px
  stack-margin: 12px
  safe-area-bottom: 32px
---

## Brand & Style
The design system is an industrial-grade interface designed for high-density information display on 1:1 OLED hardware. The personality is utilitarian, precise, and mission-critical, evoking the feel of an advanced embedded operating system.

The design style is a hybrid of **Minimalism** and **Technical Brutalism**. It prioritizes extreme legibility and power efficiency by utilizing pure black backgrounds and high-intensity safety accents. Visual elements are characterized by raw technical data, monospaced typography, and large radii that echo the physical form factor of the circular or rounded-square hardware.

## Colors
The palette is optimized for OLED longevity and high-glanceability in high-stress environments.

- **Primary (Safety Orange):** Reserved for active states, critical tasks, and primary action triggers. 
- **Surface (Pure Black):** The base layer for all screens to ensure zero-pixel power consumption in unused areas.
- **Surface-Variant (Slate Grey):** Used for structural containment, borders, and separating inactive background elements.
- **On-Surface (White):** High-contrast data points and time-keeping.

## Typography
The typography system uses a tiered approach to separate "Information" from "Status."

- **The Clock:** Uses a thin-weight sans-serif for a sophisticated, technical look that maximizes screen real estate without feeling "heavy."
- **Content:** Inter provides high legibility for task names and notifications.
- **Technical Metadata:** All IDs, status tags, and system metrics use monospaced fonts to ensure character alignment and a "machine-readable" aesthetic. Use uppercase for all monospaced labels to reinforce the industrial tone.

## Layout & Spacing
The layout is tailored for a 480x480 pixel canvas. This design system uses a **Fixed Stack** model rather than a traditional grid.

- **Bottom-Heavy Alignment:** Content is anchored to the bottom of the screen to accommodate natural thumb reach and ergonomic viewing angles.
- **Safe Zones:** A 24px inner margin is mandatory to prevent content clipping on rounded hardware corners.
- **Vertical Rhythm:** Elements are stacked vertically with tight 8px gaps to maximize the number of visible task rows.

## Elevation & Depth
Depth is communicated through **Tonal Layering** rather than shadows. Since the background is pure black (#000000), shadows are ineffective.

- **Level 0 (Base):** Pure Black background.
- **Level 1 (Inactive Card):** #1A1A1A surface with a 1px border of #2D2D2D.
- **Level 2 (Active Card):** Safety Orange (#FF5F1F) surface.
- **Interaction:** Upon press, cards should scale down slightly (98%) rather than changing color, maintaining the industrial feel of a physical "click."

## Shapes
This design system utilizes an aggressive corner radius to harmonize with the circular nature of wearable/embedded displays.

- **Containers:** All primary cards and modals use a 48px radius (`rounded-xl` in this context).
- **Small Elements:** Chips and buttons use a full "Pill" radius to contrast against the structural data.

## Components

### Task Cards
Full-width containers (Width: 432px after margins). 
- **Active State:** Solid Safety Orange background with Black text.
- **Inactive/Pending State:** Slate Grey (#1A1A1A) background with White text.
- **Layout:** Top-left contains `label-technical` for TASK_ID; Center-left for Task Title; Bottom-right for countdown timer.

### Status Bar
Fixed at the top or bottom extreme. Uses `status-bar` typography with minimalist 16px icons. Icons must be stroke-based (1.5px weight) to maintain the technical aesthetic.

### Input Fields / Buttons
Buttons are always full-width with a 48px height. Text is centered and uppercase. Input fields are represented as "slots" within cards, using a monospaced underscore cursor.

### Progress Indicators
Linear bars with 2px thickness. Background is #1A1A1A, and progress fill is #FF5F1F. Do not use rounded caps on progress bars; keep ends square for a more "plotted" look.