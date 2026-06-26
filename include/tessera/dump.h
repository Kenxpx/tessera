/* tessera/dump.h - a human-readable rendering of a bundle.
 *
 * tsb_dump() loads a bundle and writes an indented, textual view of every
 * section to `out` - the atoms, the value slots (by kind), and the node graph
 * with its typed properties and ref ops. It is the inspection path behind the
 * `tsbtool dump` command and a convenient way to eyeball a fuzz input.
 */
#ifndef TESSERA_DUMP_H
#define TESSERA_DUMP_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Load `data` and render it to `out`. Returns TSB_OK, or the loader's
 * tsb_status if the image does not parse (in which case a one-line error is
 * written to `out`). */
int tsb_dump(const uint8_t *data, size_t size, FILE *out);

#ifdef __cplusplus
}
#endif

#endif /* TESSERA_DUMP_H */
