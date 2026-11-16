import type {Cartesian2, Entity, ModelGraphics, Viewer} from "cesium";
import * as Cesium from "cesium";
import type {Accessor, Setter} from "solid-js";
import {pickEntity} from "~/lib/cesium/pickEntity";
import {Drone} from "~/lib/cesium/Drone";
import {foreverInterval} from "~/lib/cesium/intervals";
import {HistoricPathRenderer} from "~/lib/cesium/HistoricPathRenderer";

export const droneModel: ModelGraphics.ConstructorOptions = {
  uri: "drone.glb",
  // This config is responsible for keeping the drone at constant size while zooming out
  // I think this is good b/c it makes finding/clicking drones easier
  minimumPixelSize: 64,
  maximumScale: 20000,
};

export default class DronesController {
  private selectedDrone: Drone | undefined;

  /**
   * Takes the value of this.selectedDrone, converts its 3d position to 2d screen space,
   * and uses that to update the popup position. If selectedDrone == undefined, sets popupPos to undefined also
   */
  private readonly updatePopupPos: () => void;

  /**
   * Constructs a new Controller of all drones. Manages clicking on drones, tracking drone popups, and adding drones
   * @param viewer a reference to the Cesium viewer
   * @param _setPopupPos function to call when the camera moves with the drone's new screen position (used for sticky ui)
   * @param isDrawingPath Function that returns whether a path is currently being drawn (probably a Solid signal). If true, disallow selecting drones
   */
  constructor(private viewer: Viewer, _setPopupPos: Setter<Cartesian2 | undefined>, private isDrawingPath: Accessor<boolean>) {
    this.updatePopupPos = () =>
      _setPopupPos(
        this.selectedDrone ?
        Cesium.SceneTransforms.wgs84ToWindowCoordinates(viewer.scene, this.selectedDrone.position)
        : undefined,
      );

    viewer.camera.changed.addEventListener(() => {
      if (!this.selectedDrone || this.isDrawingPath()) return;
      // https://cesium.com/learn/cesiumjs/ref-doc/SceneTransforms.html
      this.updatePopupPos();
    });
  }

  /**
   * Determines if click event is on top of drone and updates state accordingly
   * @param clickPos the position the click event occurred at
   * @returns tuple:
   *     1st index: the currently selected drone or undefined if none selected
   *
   *     2nd index: true if a drone was selected/unselected, false otherwise
   */
  tryPickDrone(clickPos: Cartesian2): [Drone | undefined, boolean] {
    const pickedEntity = pickEntity(this.viewer, clickPos);
    if (pickedEntity && !this.isDrawingPath()) {  // Select drone only if no active path
      this.selectedDrone = new Drone(pickedEntity, this.viewer.clock);
      this.updatePopupPos();
      return [this.selectedDrone, true];
    }
    if (!pickedEntity && this.selectedDrone) {  // Unselect drone, don't start drawing path
      this.selectedDrone = undefined;
      this.updatePopupPos();
      return [this.selectedDrone, true];
    }
    return [this.selectedDrone, false];
  }

  /**
   * Add a drone to the Cesium scene at provided location
   * @param id identifier of this drone in the database
   * @param longitude GPS longitude
   * @param latitude GPS latitude
   * @param height distance from WGS84 reference ellipsoid to place drone (what you get from GPS)
   * @param heading compass direction (deg) to point in. 0 is north, increase clockwise
   * @see https://sandcastle.cesium.com/?src=3D%20Models.html
   */
  addDrone(id: number, latitude: number, longitude: number, height: number, heading: number) {
    // TODO: ignore scene lighting, hard to see at night
    const positionProp = new Cesium.SampledPositionProperty();
    positionProp.forwardExtrapolationType = Cesium.ExtrapolationType.HOLD;
    return new Drone(this.viewer.entities.add({
        model: droneModel,
        properties: {id},
        availability: foreverInterval,
        position: positionProp,
      } satisfies Entity.ConstructorOptions), this.viewer.clock, new HistoricPathRenderer(this.viewer.entities, id))
      .setPos(
        latitude, longitude, height, heading,
      );
  }
}
