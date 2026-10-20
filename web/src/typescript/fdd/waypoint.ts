/*
 *  Waypoint display format. Flight plans arrive in many styles (52/50, 52N050W, 5230N05000W, 4545N ...);
 *  the display always shows oceanic coordinates the same short way, as on the NAT tracks: 52/50.
 *  Always whole degrees (4 digits): minutes are dropped, so 4730N05000W is shown as 47/50.
 *  Named fixes are returned unchanged (upper case, without any /M082F390 speed-level suffix).
 *  Anything that is not a north-west coordinate (e.g. east longitudes) is returned unchanged.
 */
export function formatWaypoint(token: string): string {
  const t: string = (token ?? "").trim().toUpperCase().replace(/\/[NMK]\d{3,4}[FASM]\d{3,4}$/, "");
  const deg = (s: string): string => String(parseInt(s, 10)).padStart(2, "0");

  // 52/50, 5230/50, 52/050
  let m = t.match(/^(\d{2})(?:\d{2})?\/(\d{2,3})$/);
  if (m) return deg(m[1]) + "/" + deg(m[2]);

  // 52N050W, 5230N05000W, 52N05030W
  m = t.match(/^(\d{2})(?:\d{2})?N(\d{3})(?:\d{2})?W$/);
  if (m) return deg(m[1]) + "/" + deg(m[2]);

  // ICAO shorthand 4545N = 45N 045W (the letter N means north-west)
  m = t.match(/^(\d{2})(\d{2})N$/);
  if (m) return deg(m[1]) + "/" + deg(m[2]);

  return t;
}
