import {Typography} from "@suid/material";

import {graphql} from "~/gql";
import {createSubscription} from "@merged/solid-apollo";
import {type JSX, Match, Show, Switch} from "solid-js";

const warningsQuery = graphql(`
    subscription DroneWarnings($droneId: bigint!) {
        me: drones_by_pk(drone_id: $droneId) {
            telemetry: drone_telemetries(limit: 1, order_by: {timestamp: desc}) {
                battery
                # stage_of_flight
                timestamp
            }
            flights(limit: 1, order_by: {flight_id: desc}) {
                status
            }
        }
    }
`);

const CONCERNING_LAG = 5000;  // in ms

/**
 * Show various human-readable critical information. Ex: long time w/o update, low battery, crashed
 * @param props.id drone id to show warnings for
 * @param props.ok Show this element if there are no warnings
 * @param props
 */
export default function DroneWarnings(props: {id: number | string, ok?: JSX.Element}) {
  const droneWarnings = createSubscription(warningsQuery, {variables: {droneId: props.id}});
  const telemetry = () => droneWarnings()?.me?.telemetry[0];
  const flight = () => droneWarnings()?.me?.flights[0];
  const timeSinceUpdate = () => new Date().getTime() - telemetry()?.timestamp ?? 0;

  return (
    <Show
      when={telemetry() && flight()}
    >
      <Switch fallback={props.ok}>
        <Match when={timeSinceUpdate() > CONCERNING_LAG}>
          <Typography>🚨 {Math.floor(timeSinceUpdate() / 1000)} seconds since last update</Typography>
        </Match>
        <Match when={telemetry()!.battery < 20}>
          <Typography>⚠️ Low battery</Typography>
        </Match>
        <Match when={telemetry()!.stage_of_flight !== "idle" && flight()!.status === "failed"}>
          <Typography>🚨 Crashed</Typography>
        </Match>
      </Switch>
    </Show>
  );
}
