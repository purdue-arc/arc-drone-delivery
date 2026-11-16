import * as Cesium from "cesium";
import {Cartesian3, JulianDate} from "cesium";

declare module "cesium" {
  interface SampledPositionProperty {
    _property: Cesium.SampledProperty,
  }
  // See https://github.com/CesiumGS/cesium/blob/main/packages/engine/Source/DataSources/SampledProperty.js
  // Could break if updating Cesium
  interface SampledProperty {
    _times: JulianDate[],
    _values: number[],
    _updateTableLength: boolean,
    _definitionChanged: Event,
  }
}

export default class KeyedTimePositionProperty {
  /**
   * Property which returns the coordinates of all samples as they are updated. Automatically mirrors state of private `getPositions`
    @see getPositions */
  public readonly positionProp = new Cesium.CallbackProperty(this.getPositions.bind(this), false);
  /** Property that contains time and position of flight path */
  public readonly pathProp = new Cesium.SampledPositionProperty();

  /**
   * Mapping of ids to indexes of `times` or `positions` (times 3 b/c packing).
   * The index of this array is the id, and its value is the corresponding index. Append only.
   */
  private readonly idIndexMap: number[] = [];
  /** Reference to array of sample times inside `SampledPositionProperty` */
  private readonly times: JulianDate[] = this.pathProp._property._times;
  /** Reference to packed array of sample `Cartesian3`s inside `SampledPositionProperty` */
  private readonly positions: number[] = this.pathProp._property._values;
  /** The time at which the simulation starts (read-only, use setter) */
  private _startTime = new JulianDate();
  /** Unpacked representation of `positions`, used by `CallbackProperty` */
  private getPositions() {
    return Cesium.Cartesian3.unpackArray(this.positions);
  }

  /** Change the start time (first sample) and all following samples */
  set startTime(newTime: JulianDate) {
    if (newTime.equals(this._startTime)) return;
    this._startTime = newTime;
    this.upsertSample({idx: 0});
  }

  /**
   * Manages adding, editing, and deleting samples with constant identifiers from 2 mirroring cesium properties: with and without time info.
   * Also maintains constant velocity between points
   * @param speed m/s, used to compute time between points
   */
  constructor(public readonly speed: number) {
    this.pathProp.backwardExtrapolationType =
    this.pathProp.forwardExtrapolationType =
      Cesium.ExtrapolationType.NONE;
  }

  /**
   * Set the `positionProp` callback property's constant field to the newly provided constant value.
   * @param isConstant true = path is dynamic as points added. False = path is not dynamic, but more performant as function not called repeatedly
   */
  setIsConstant(isConstant: boolean) {
    this.positionProp.setCallback(this.getPositions.bind(this), isConstant);
  }

  /**
   * Add or update a sample, updating all after to maintain constant velocity. (`id` or `idx`) or `newPosition` must always be supplied
   * @param newPosition new world position of sample (defaults to position at `idx`)
   * @param id constant identifier to lookup index of sample to modify. No default
   * @param idx index of entry to modify. Defaults to item index specified by `id`
   */
  private upsertSample({newPosition, id, idx}: {newPosition?: Cartesian3, id?: number, idx?: number}) {
    // Recursive base case
    if (idx != undefined && idx >= this.times.length)
      return;

    // Default params
    idx = idx ?? this.idIndexMap[id as number] ?? this.times.length;
    newPosition = newPosition ?? Cartesian3.unpack(this.positions, idx * 3);

    // Compute new time with constant velocity
    const prevPos = this.positions[(idx - 1) * 3] != undefined ? Cartesian3.unpack(this.positions, (idx - 1) * 3) : newPosition;
    const prevTime = this.times[idx - 1] ?? this._startTime;
    const deltaTime = Cartesian3.distance(prevPos, newPosition) / this.speed;
    const elapsedTime = JulianDate.addSeconds(
      prevTime,
      deltaTime,
      new Cesium.JulianDate(),
    );

    if (this.times[idx] == undefined) {  // Add
      this.times.push(elapsedTime);
      this.idIndexMap.push(idx);
    } else {  // Update
      this.times[idx] = elapsedTime;
      this.upsertSample({idx: idx + 1});
    }
    Cartesian3.pack(newPosition, this.positions, idx * 3);

    // emit events to change property
    this.pathProp._property._updateTableLength = true;
    this.pathProp._property._definitionChanged.raiseEvent(this.pathProp._property);
  }

  /**
   * Returns true if id is valid, false otherwise
   * @param id
   */
  private doesntExists(id: number) {
    const index = this.idIndexMap[id];
    return index == undefined || index < 0;
  }

  /**
   * Adds a new sample (keyframe) to both `pathProp` (contains timing info, in air) and `positionProp` (no timing info, on ground)
   * @param position The position sample to append
   */
  addSample(position: Cartesian3) {
    this.upsertSample({newPosition: position});
    return this.idIndexMap.length - 1;
  }

  /**
   * Update position of sample `id` and all times after to maintain constant velocity
   * @param id id of sample to update
   * @param newPosition The new position sample to replace with
   * @returns true if operation successful, false otherwise (id doesn't exist)
   */
  editSample(id: number, newPosition: Cartesian3) {
    if (this.doesntExists(id))
      return false;
    this.upsertSample({id, newPosition});
    return true;
  }

  /**
   * Delete sample specified by `id` and update all times after to maintain constant velocity
   * @param id id of sample to remove
   * @returns true if operation successful, false otherwise (id doesn't exist)
   */
  removeSample(id: number) {
    if (this.doesntExists(id))
      return false;
    const idx = this.idIndexMap[id];
    this.times.splice(idx, 1);
    this.positions.splice(idx * 3, 3);

    // Adjust idIndexMap so it points to the correct indices
    this.idIndexMap[id] = -1;
    for (let i = id + 1; i < this.idIndexMap.length; i++) {
      this.idIndexMap[i]--;
    }

    // update succeeding samples
    this.upsertSample({idx});
    return true;
  }
}
