# Supplying artwork

You can supply artwork with a documented basis for redistribution. Keep the
creator, original source, licence or permission reference, required attribution,
and hash of the exact file in the game's artwork.json. Permission records are
maintainer assertions, not automatic legal verification.

Good candidates are original artwork you own, assets under a licence permitting
the intended redistribution, or assets covered by express permission from the
rights holder. Check any included logos, characters and other third-party
elements too. Creator credit, small file size, a disclaimer, or hosting elsewhere
does not by itself establish permission.

## Local artwork while permission is unconfirmed

~~~sh
node scripts/import-artwork.mjs --game games/nimauria/project-gracemeria --file /path/to/cover.webp --role cover --local
~~~

This preserves the file under .local/artwork/<username>/<game>/ without adding it
to the catalogue, install package or Git. Use the launcher's existing local
artwork import controls for a personal library image. This local storage does
not grant a right to publish the file later.

The two existing publisher images were marked local-only after the maintainer
confirmed that redistribution permission is unconfirmed. Their originals are
preserved locally under .local/artwork/nimauria/project-gracemeria/.

## Artwork with redistribution permission

~~~sh
node scripts/import-artwork.mjs --game games/nimauria/project-gracemeria --file /path/to/original-cover.png --role cover --creator "Artist name" --source "Original source reference" --license "Licence name or permission terms" --basis original --evidence "Public licence URL or permission reference" --attribution "Required credit"
~~~

Use basis original, licensed or permission as appropriate. Use role hero for a
background. PNG, JPEG and WebP are accepted up to 8 MiB. The command copies the
file to assets/, updates the catalogue reference, artwork.json, distribution
list and package hashes. Keep confidential permission correspondence outside
the public repo; record a reference and retain the evidence privately.

Run node scripts/validate-catalog.mjs --publish before submission. Publication
checks reject images lacking a complete permitted record, including unreferenced
images in assets/. Replacing image bytes invalidates the old permission record.
CI cannot establish that the contributor actually holds the stated rights;
maintainers still review the licence, source and evidence.

These records reduce accidental inclusion; they do not guarantee immunity from
claims. GitHub may request changes or disable access following a valid complaint:
https://docs.github.com/en/site-policy/content-removal-policies/dmca-takedown-policy

Removing images from a new commit does not remove older Git history. This change
does not rewrite published history. Any history cleanup requires separate review
and coordination. Apply the same distribution-rights review to module binaries.
