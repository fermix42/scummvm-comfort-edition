# Comfort Edition logo

`comfort-edition-logo.png` is the owner-supplied artwork from November 19, 2026.
Keep this source unchanged. The theme BMPs in `gui/themes/common` are proportional
Lanczos resizes: `comfort-logo.bmp` (240 x 80) and
`comfort-logo-small.bmp` (120 x 40). They fit the existing launcher logo slots.

The source has a black matte. ThemeEngine treats black and near-black resampling
fringes as transparent for these two named assets only. Other bitmaps retain the
existing magenta transparency key. The three image-based bundled themes declare
these assets directly, including the remastered themes, so their old SVG logos
cannot override the supplied artwork. Rebuild theme ZIPs with `scummtheme.py
makeall` before running the approved clean-environment executable build.
