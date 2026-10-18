# Theme gallery

The themes shown in SCANCORE under Settings → Theme → Gallery. The app reads
`index.json` from this folder on the `main` branch, so a theme added here
appears for everyone without a new release.

## Adding a theme

1. In SCANCORE, make your theme (Settings → Theme → Custom), give it a name
   and choose **Export** to save a `.sct` file.
2. Copy it here with a short lowercase file name, such as `night-market.sct`.
3. Add an entry to `index.json`:

   ```json
   {
     "id": "night-market",
     "name": "Night Market",
     "author": "your name",
     "description": "One short sentence.",
     "window": "#16121c",
     "accent": "#ffcf5a",
     "file": "night-market.sct"
   }
   ```

   `window` and `accent` are the colours in the file; the gallery draws its
   card from them. A theme with a background picture should also set
   `"background": true` and add a 320 × 200 JPEG as `"preview"`.
4. Open a pull request.

Keep files small (a background picture of a few hundred KB at most) and only
share pictures you made or may share.
