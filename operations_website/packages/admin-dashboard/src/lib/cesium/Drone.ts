import type {Cartesian3, Clock, Entity} from "cesium";
import * as Cesium from "cesium";
import PathController from "~/lib/cesium/PathController";
import {HistoricPathRenderer} from "~/lib/cesium/HistoricPathRenderer";


export interface DroneProperties {
  id: number,
  path?: PathController,
}


export class Drone {
  /**
   * Wrapper around Cesium.Entity for Drones
   * @param entity Drone Entity to wrap
   * @param clock Clock instance for Viewer which this Drone belongs. Used for getting position
   * @param historicPathRenderer object that will render the historic path of this drone
   */
  constructor(private readonly entity: Entity, private readonly clock: Clock, private readonly historicPathRenderer?: HistoricPathRenderer) {
  }

  get position() {
    return this.entity.position!.getValue(this.clock.currentTime) as Cartesian3;
  }

  get id() {
    return this.getProps().id;
  }

  set extrapolation(type: Cesium.ExtrapolationType) {
    (this.entity.position as Cesium.SampledPositionProperty).forwardExtrapolationType = type;
  }

  /**
   * Change position of a Cesium entity
   * @param longitude GPS longitude
   * @param latitude GPS latitude
   * @param height distance from WGS84 reference ellipsoid to place drone (what you get from GPS)
   * @param heading compass direction (deg) to point in. 0 is north, increase clockwise
   * @param time when was the drone at this position? Defaults to current timeline position
   * @see addDrone
   */
  setPos(latitude: number, longitude: number, height: number, heading: number, time = this.clock.currentTime) {
    // Cesium uses WGS84 reference ellipsoid (https://epsg.org/ellipsoid_7030/WGS-84.html), same as GPS (https://community.cesium.com/t/get-heighy-value/23064/2)
    // See https://www.unavco.org/education/resources/tutorials-and-handouts/tutorials/geoid-gps-receivers.html for more info on how GPS works
    // Avg height of Purdue is 190m. Cesium renders terrain at the correct height. Check specific height: https://www.daftlogic.com/projects-find-elevation-on-map.htm
    const position = Cesium.Cartesian3.fromDegrees(longitude, latitude, height);
    const hpr = new Cesium.HeadingPitchRoll(Cesium.Math.toRadians(heading - 90), 0, 0);
    const orientation = Cesium.Transforms.headingPitchRollQuaternion(
      position,
      hpr,
    );
    // I tried adding a second to clock.currentTime in an attempt to smooth the path, but jumping still occurred & was worse than using db timestamp.
    // Guessing need to add new sample at extrapolated point & another 200ms ahead at new point, but not worth it
    (this.entity.position as Cesium.SampledPositionProperty).addSample(time, position);
    // @ts-ignore This error can probably be ignored, seems to work
    this.entity.orientation = orientation;

    this.historicPathRenderer?.addWaypoint(position);
    return this;
  }

  select() {
    this.historicPathRenderer?.showFullHistory();
  }

  deselect() {
    this.historicPathRenderer?.hideFullHistory();
  }

  /**
   * Gets custom properties for a Drone at the current time as the correct type
   */
  getProps() {
    console.log(this.entity?.properties?.id);
    return this.entity.properties!.getValue(new Cesium.JulianDate()) as DroneProperties;
  }

  setProp<T extends keyof DroneProperties>(key: T, value: DroneProperties[T]) {
    // TODO: can I use addProperty for both adding & updating?
    this.entity.properties!.addProperty(key, value);
  }
}
