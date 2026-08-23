# Third-party notices

`igui_font` obtains two build-time inputs from Nuklear at commit
`0dbc52f86404f9e1f26ce0df3015ed23ff54a726`:

- `src/stb_truetype.h`, used to rasterize the font atlas. It is supplied by
  Nuklear under its MIT or public-domain licensing choice; see Nuklear's
  [`LICENSE`](https://github.com/Immediate-Mode-UI/Nuklear/blob/master/LICENSE).
- `extra_font/Roboto-Regular.ttf`, embedded into the resulting binary. Roboto
  is distributed under the Apache License 2.0 by Google.

The font file is fetched only while configuring `IGUI_BUILD_FONT`; it is not
copied into this repository.
