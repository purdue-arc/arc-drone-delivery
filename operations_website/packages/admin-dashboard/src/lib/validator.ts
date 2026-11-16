/**
 * Cartographic point with latitude and longitude in degrees and altitude in meters
 */
export interface CartoDegrees {
  latitude: number;
  longitude: number;
  altitude: number;
}

/**
 * Validates a point along a route
 * @param point point to validate
 */
export function validatePoint(point: CartoDegrees): string | null {
  const valid = point.latitude >= 40.427919 && point.latitude <= 40.431545 && point.longitude >= -86.931237 && point.longitude <= -86.92643;
  return valid ? null : "point out of bounds";
}
