import {Typography} from "@suid/material";
import {graphql} from "~/gql";
import {createSubscription} from "@merged/solid-apollo";
import {Show} from "solid-js";

const destinationQuery = graphql(`
    subscription DroneDestination($droneId: bigint!) {
        me: drones_by_pk(drone_id: $droneId) {
            telemetry: drone_telemetries(limit: 1, order_by: {timestamp: desc}) {
                # stage_of_flight
                has_package
            }
            flights(limit: 1, order_by: {flight_id: desc}) {
                order {
                    vendor {
                        name
                    }
                }
            }
        }
    }
`);

/**
 * Display human-readable movement information. Ex: Delivering to Pete's Za, picking up from Panera.
 * @param props.id drone id to show movement for
 */
export default function ReadableDestination(props: {id: number | string}) {
  const droneDestination = createSubscription(destinationQuery, {variables: {droneId: props.id}});
  const telemetry = () => droneDestination()?.me?.telemetry[0];
  const flight = () => droneDestination()?.me?.flights[0];

  return (
    <Show when={telemetry() && flight()}>
      <Show when={flight()!.order} fallback={<Typography>{telemetry()!.stage_of_flight}</Typography>}>
        {/* TODO: intelligently report states such as: Delivering to ___/picking up from _____/parking at */}
        <Typography>{telemetry()!.has_package ? "Delivering" : "Picking up"} from {flight()!.order!.vendor!.name}</Typography>
      </Show>
    </Show>
  );
}
