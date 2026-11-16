import * as Cesium from "cesium";

/**
 * Converts a Cartesian3 to (long, lat, height), adds to the height, and converts back to new Cartesian3.
 * Necessary because Cartesian3.z += d will move in Euclidean space, not relative to ground.
 * @param point Cartesian 3 point to modify (immutable, returns copy)
 * @param height height to add in meters
 */
export function addHeight(point: Cesium.Cartesian3, height: number) {
  const cartographic = Cesium.Cartographic.fromCartesian(point);
  cartographic.height += height;
  return Cesium.Cartesian3.fromRadians(cartographic.longitude, cartographic.latitude, cartographic.height);
}
