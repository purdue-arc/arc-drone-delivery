import * as Cesium from "cesium";
import {type Cartesian3, Cartographic} from "cesium";
import KeyedTimePositionProperty from "~/lib/cesium/KeyedTimePositionProperty";
import {droneModel} from "~/lib/cesium/DronesController";
import {graphql} from "~/gql";
import {createMutation} from "@merged/solid-apollo";
import {foreverInterval} from "~/lib/cesium/intervals";
import type {Drone} from "~/lib/cesium/Drone";
import {Accessor, createSignal, Setter} from "solid-js";


const simulateTelemetryMutation = graphql(`
    mutation SimulateTelemetry($drone_id: bigint, $heading: float8, $timestamp: bigint, $position: geography) {
        insert_drone_telemetry_one(object: {drone_id: $drone_id, battery: "50", has_package: false, heading: $heading, timestamp: $timestamp, position: $position}) {
            id
        }
    }
`);


export default class PathController {
  /** Position interpolator: computes position between time and position keyframes */
  private property: KeyedTimePositionProperty;
  /** Id of the currently active preview node */
  private previewId = -1;
  /** Cesium entity corresponding to the yellow path in the sky */
  private skyPathEntity: Cesium.Entity | undefined;
  /** Cesium entity corresponding to the white path on the ground */
  private groundPathEntity: Cesium.Entity | undefined;
  /** Yellow circles in the sky allowing for adjusting sky path */
  private skyWaypointEntities: Cesium.Entity[] = [];
  /** Setter for reactive sky waypoints in longitude/latitude form */
  private readonly setWaypoints: Setter<Cartographic[]>;
  /** Reactive sky waypoints in longitude/latitude form */
  public readonly waypoints: Accessor<Cartographic[]>;
  /** Red dots on the ground visualizing where your cursor is */
  private groundWaypointEntities: Cesium.Entity[] = [];
  /** javascript interval id when simulating on database */
  private simulationInterval: number | undefined;
  /** Because of this, class must be instanced at top-level component. Only need 1 instance */
  private readonly addTelemetry = createMutation(simulateTelemetryMutation)[0];
  /** The Drone owner of the current path */
  public drone!: Drone;

  /**
   * Constructs a new PathController that manages previewing flight paths
   * @param viewer a reference to the Cesium viewer
   * @param droneSpeed in meters per second. Used to compute keyframe timing assuming straight line
   */
  constructor(private viewer: Cesium.Viewer, public readonly droneSpeed: number) {
    this.property = new KeyedTimePositionProperty(droneSpeed);
    [this.waypoints, this.setWaypoints] = createSignal([] as Cartographic[]);
  }

  /**
   * Start a new flight path. Resets internal state & adds path entity to scene
   * @param drone The Drone owner of the new path
   */
  beginPath(drone: Drone) {
    this.property = new KeyedTimePositionProperty(this.droneSpeed);
    this.previewId = -1;
    this.drone = drone;
    this.skyWaypointEntities = [];
    this.groundWaypointEntities = [];
    this.setWaypoints([]);
    this.skyPathEntity = this.viewer.entities.add({
      //Set the entity availability to the same interval as the simulation time.
      availability: foreverInterval,

      position: this.property.pathProp,
      orientation: new Cesium.VelocityOrientationProperty(this.property.pathProp),

      //Show the path sampled in 1 second increments.
      path: {
        resolution: 1,
        material: new Cesium.PolylineGlowMaterialProperty({
          glowPower: 0.1,
          color: Cesium.Color.YELLOW,
        }),
        width: 10,
      },
    });
    // TODO: test if it's possible/necessary to round corners. This attempt veered too far off course
    // this.property.pathProp.setInterpolationOptions({
    //   interpolationDegree: 2,
    //   interpolationAlgorithm: Cesium.HermitePolynomialApproximation,
    // });

    // Add the white path on the ground
    this.groundPathEntity = this.viewer.entities.add({
      polyline: {
        positions: this.property.positionProp,
        clampToGround: true,
        width: 3,
      },
    });
  }

  /**
   * Add a new keyframe to the flight path. Computes time by assuming constant velocity linear flight path based on `droneSpeed`.
   * Also Creates a tiny red dot which follows your cursor when creating a new flight path
   * @param position new position to visit
   * @return Entity for the red point on the ground
   */
  extendPath(position: Cartesian3) {
    this.previewPath(position);
    this.previewId = -1;

    //Also create a point for each sample we generate.
    // TODO: allow clicking through handles & line when placing points, otherwise can cause weird stacking
    this.skyWaypointEntities.push(this.viewer.entities.add({
      position,
      point: {
        pixelSize: 8,
        color: Cesium.Color.TRANSPARENT,
        outlineColor: Cesium.Color.YELLOW,
        outlineWidth: 3,
      },
    }));

    const groundPoint = this.viewer.entities.add({
      position,
      point: {
        color: Cesium.Color.RED,
        pixelSize: 5,
        heightReference: Cesium.HeightReference.CLAMP_TO_GROUND,
      },
    });
    this.groundWaypointEntities.push(groundPoint);
    this.setWaypoints(this.skyWaypointEntities.map(e =>
      Cartographic.fromCartesian(e.position!.getValue(new Cesium.JulianDate()) as Cartesian3)
    ));

    return groundPoint;
  }

  /**
   * Remove current preview node, then add a new keyframe to path, one second from `elapsedTime` so it can be easily found.
   * Also doesn't render adjustment handle (circle)
   * @param position new position to preview
   */
  previewPath(position: Cartesian3) {
    if (this.previewId >= 0) {
      console.assert(this.property.editSample(this.previewId, position));
    } else {  // previewId == -1
      this.previewId = this.property.addSample(position);
    }
  }

  /** Removes preview path node */
  closePath() {
    this.property.removeSample(this.previewId);
    this.property.setIsConstant(false);
  }

  /** Remove preview entities from screen & stop simulating */
  clearPath() {
    this.endSimulation();
    if (!this.skyPathEntity)
      throw "No path to clear";
    this.viewer.entities.remove(this.skyPathEntity);
    this.viewer.entities.remove(this.groundPathEntity!);  // Defined together with skyPathEntity
    this.skyWaypointEntities.forEach(e => this.viewer.entities.remove(e));
    this.groundWaypointEntities.forEach(e => this.viewer.entities.remove(e));
  }

  /** Simulate the created drone path by animating a drone along its path (does not write to database) */
  simulateLocal() {
    if (!this.skyPathEntity)
      throw "No path to simulate";
    this.property.startTime = this.viewer.clock.currentTime;
    this.skyPathEntity.model = new Cesium.ModelGraphics(droneModel);
  }

  /** Simulate the created drone path by sending fake telemetry events to the database */
  simulateDatabase() {
    this.simulateLocal();
    this.drone.extrapolation = Cesium.ExtrapolationType.EXTRAPOLATE;
    const simulationFrequency = 1000;
    this.simulationInterval = window.setInterval(this.simulateDatabaseTick.bind(this), simulationFrequency);
  }

  /** Call every time interval when simulating on database*/
  private simulateDatabaseTick() {
    console.log("tick...");
    const pos = this.property.pathProp.getValue(this.viewer.clock.currentTime);
    if (pos == undefined) {
      this.endSimulation();
      return;
    }
    const gps = Cesium.Cartographic.fromCartesian(pos);
    this.addTelemetry({variables: {
      heading:
        Cesium.HeadingPitchRoll.fromQuaternion(
          this.skyPathEntity!.orientation!.getValue(this.viewer.clock.currentTime) as Cesium.Quaternion
        ).heading * Cesium.Math.DEGREES_PER_RADIAN + 90,
      position: {
        "type": "Point",
        "coordinates": [
          Cesium.Math.toDegrees(gps.latitude),
          Cesium.Math.toDegrees(gps.longitude),
          gps.height
        ],
      },
      drone_id: this.drone.id,
      timestamp: Cesium.JulianDate.toDate(this.viewer.clock.currentTime).getTime(),
    }});
  }

  /** Stop simulation & clean up objects. If no active path, log a warning, not an exception */
  endSimulation() {
    if (this.skyPathEntity) {
      this.skyPathEntity.model = undefined;
      window.clearInterval(this.simulationInterval);
      this.drone.extrapolation = Cesium.ExtrapolationType.HOLD;
    } else
      console.warn("No simulation to end");
  }
}
