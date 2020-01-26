/* credit: Steffen Kieß (https://stackoverflow.com/questions/55641889/access-build-id-at-runtime) */

__build_id_start = ADDR(.note.gnu.build-id);
__build_id_end = ADDR(.note.gnu.build-id) + SIZEOF(.note.gnu.build-id);

__interp_start = ADDR(.interp);
__interp_end = ADDR(.interp) + SIZEOF(.interp);
