import {Dynamic} from "solid-js/web";
import {graphql} from "~/gql";
import {createSubscription} from "@merged/solid-apollo";
import {
  Battery0Bar,
  Battery1Bar,
  Battery2Bar,
  Battery3Bar,
  Battery4Bar,
  Battery5Bar,
  Battery6Bar,
  BatteryFull,
} from "@suid/icons-material";
import LabeledIcon from "~/components/generic/LabeledIcon";

const batteryQuery = graphql(`
    subscription Battery($droneId: bigint!) {
        me: drones_by_pk(drone_id: $droneId) {
            telemetry: drone_telemetries(limit: 1, order_by: {timestamp: desc}) {
                battery
            }
        }
    }
`);

const batteryStatus = [Battery0Bar, Battery1Bar, Battery2Bar, Battery3Bar, Battery4Bar, Battery5Bar, Battery6Bar, BatteryFull];


/**
 * Show battery info for specified drone, with dynamic icon (changes depending on charge)
 * @param props.id id of drone to show battery info for
 */
export default function BatteryFragment(props: {id: number | string}) {
  const droneWarnings = createSubscription(batteryQuery, {variables: {droneId: props.id}});
  const battery = () => droneWarnings()?.me?.telemetry[0]?.battery;

  return (
    <LabeledIcon title="Battery charge">
      <Dynamic component={batteryStatus[Math.round((battery() ?? 0) / 100 * (batteryStatus.length - 1))]} sx={{ marginRight: "0.5em" }} />
      {battery()}%
    </LabeledIcon>
  );
}
