# Compressed world-window entry493

Status: IN PROGRESS. Contract before production changes.

The navigation pass exposed an independent implementation restriction:
World.travel accepts every byte coordinate on compressed maps, and ordinary
WorldStory.advanceWindow already wraps authored sector indices as bytes, but
WorldStory.initialize rejects sector columns/rows0 and15. Its unsigned neighbor
expression would also pass out-of-range indices to MapResource.sector there.
That makes an otherwise valid travel destination fail at the window phase.

Complete the existing loaded-window owner, using the same authored byte-sector
arithmetic as ordinary scrolling and BS.ROUTINES.BK2 neighbors28D3..28DB:
17,16,15,1,0,-1,-15,-16,-17 (the last four encoded with high bit set).
Allow all16x16 compressed centers, wrap each neighboring sector index to byte,
and retain modulo-byte window origins. Keep uncompressed local48x48 geometry,
exact exit thresholds, physical collision, map records and NSWN1 unchanged.
This supplies actual sector data; it adds no fabricated terrain or padding.

One focused batch checks compressed map outer centers against the immutable
nine-neighbor sector data, window/project/codec invariants, local geometry,
and actual WorldInput C/slot/caster/orb -> shared travel -> destination frame
and subsequent free G. Declare orb binding/spell before the first input; do
not inject outcomes. Core/window/input codecs are scoped evidence, not normal
Engine cold saves/sessions or campaign progress. Preserve accepted492 and473;
do not rerun their suites. One coherent build after implementation.
