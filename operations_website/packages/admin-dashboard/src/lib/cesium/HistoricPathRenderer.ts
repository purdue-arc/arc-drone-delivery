import type {Cartesian3, Entity, EntityCollection} from "cesium";
import * as Cesium from "cesium";
import {graphql} from "~/gql";
import {createLazyQuery} from "@merged/solid-apollo";

const droneHistoryQuery = graphql(`
  query DroneHistory($id: bigint, $limit: Int) {
    drone_telemetry(where: {drone_id: {_eq: $id}}, order_by: {timestamp: desc}, limit: $limit) {
      heading   
      position
      timestamp
    }
  }
`);

export class HistoricPathRenderer {
  /** Color used to render this drone's path */
  private readonly color;

  /** Limit on number of historic position points to show when drone unselected */
  private readonly historyLimit = 6;

  /** Limit on number of historic position points to show when drone selected */
  private readonly fullHistoryLimit = 30;

  /** Entities corresponding to the previous drone waypoints. Max length `historyLimit` or `fullHistoryLimit` depending on whether full history is showing */
    // TODO: implement this as a deque for max efficiency
  private readonly history = [] as Entity[];

  /** Lazy Apollo query to fetch drone history */
  private readonly runHistoryQuery;

  /** Entity corresponding to sky polyline */
  private shouldShowFull = false;

  /** Property for rendering sky polyline positions */
  private positionProp = new Cesium.CallbackProperty(this.cvtHistoryPoints.bind(this), false);

  /**
   * Display a few dots for the historical path of the drone
   * @param entities likely `viewer.entities` to add dots to
   * @param droneId id of drone owner
   */
  constructor(private readonly entities: EntityCollection, private readonly droneId: number) {
    this.color = Cesium.Color.fromRandom({ alpha: 1.0 });
    this.runHistoryQuery = createLazyQuery(droneHistoryQuery, {
      variables: {
        id: this.droneId
      }
    })[0];
    this.entities.add({
      polyline: {
        positions: this.positionProp,
        width: 3,
        material: new Cesium.PolylineOutlineMaterialProperty({
          outlineColor: this.color,
          color: this.color,
        }),
      },
    });
    this.fetchHistory(this.historyLimit).then(history => history.forEach(pt => this.addWaypoint(pt)));
  }

  /** Fetch the specified number of historical points and convert to Cartesian3 */
  private async fetchHistory(limit: number) {
    return (await this.runHistoryQuery({variables: {limit}}))
      .drone_telemetry.map(point =>
        Cesium.Cartesian3.fromDegrees(
          point.position.coordinates[1],
          point.position.coordinates[0],
          point.position.coordinates[2]
        )
      );
  }

  /** Convert history entities to Cartesian3 */
  private cvtHistoryPoints() {
    return this.history.map(e => e.position!.getValue(new Cesium.JulianDate()) as Cartesian3);
  }

  addWaypoint(position: Cartesian3, atStart = false) {
    this.history[atStart ? "unshift" : "push"](
      this.entities.add({
        position,
        point: {
          pixelSize: 8,
          color: this.color,
          outlineColor: Cesium.Color.TRANSPARENT,
        },
      })
    );
    if (this.history.length > (this.shouldShowFull ? this.fullHistoryLimit : this.historyLimit))
      console.log(this.entities.remove(this.history.shift()!));
  }

  /** Show previous `fullHistoryLimit` waypoints & extend line */
  showFullHistory() {
    this.shouldShowFull = true;
    this.fetchHistory(this.fullHistoryLimit).then(history => history.forEach(pt => this.addWaypoint(pt, true)));
  }

  /** Remove waypoints up to `historyLimit` & shorten line */
  hideFullHistory() {
    this.shouldShowFull = false;
    while (this.history.length > this.historyLimit)
      this.entities.remove(this.history.shift()!);
  }
}
