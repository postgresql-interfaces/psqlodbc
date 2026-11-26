# psqlODBC website

The source of the psqlODBC website, built with [Hugo](https://gohugo.io/) and the [Hextra](https://imfing.github.io/hextra/) theme. Hextra is a Hugo module whose version is pinned in `go.mod`.

## Building

You need Hugo extended 0.146 or later, and Go, which Hugo uses to download the theme.

```sh
cd website
hugo server        # serves the site at http://localhost:1313/
hugo --minify      # builds the site into public/
```

## Where the content comes from

- `content/` holds the pages.
- The changelog is `Changelog.md` at the top of the repository, which `hugo.yaml` mounts at `/changelog/`.
- The images and `.reg` files come from `docs/`, which the Windows installer and the source tarball also ship.

Pages that moved keep their old addresses through `aliases` in their front matter, so a link such as `/docs/config.html` redirects to `/docs/configuration/`.

## Releasing

Merging to `main` does not publish the site. [RELEASING.md](RELEASING.md) describes how to release it to https://odbc.postgresql.org/.
