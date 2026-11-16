import * as Cesium from "cesium";
import {type Cartesian2, type Viewer} from "cesium";

/**
 * Returns the top-most entity at the provided window coordinates or undefined if no entity is at that location.
 * @param viewer a reference to the Cesium viewer
 * @param windowPosition The window coordinates.
 * @returns The picked entity or undefined.
 * @see https://cesium.com/learn/cesiumjs-learn/cesiumjs-creating-entities/#picking
 */
export function pickEntity(viewer: Viewer, windowPosition: Cartesian2) {
  const picked = viewer.scene.pick(windowPosition);
  if (Cesium.defined(picked)) {
    const id = Cesium.defaultValue(picked.id, picked.primitive.id);
    if (id instanceof Cesium.Entity) {
      return id;
    }
  }
  return undefined;
}
