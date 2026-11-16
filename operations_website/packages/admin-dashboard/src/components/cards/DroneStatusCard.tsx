import {
  ExploreOutlined as CompassIcon,
  PlaceOutlined as PlaceIcon,
  SpeedOutlined as SpeedIcon,
} from "@suid/icons-material";
import {Box, Button, Card, Grid, Stack, Typography} from "@suid/material";
import {useParams} from "@solidjs/router";
import {graphql} from "~/gql";
import {createSubscription} from "@merged/solid-apollo";
import {createEffect, Show} from "solid-js";
import DroneWarnings from "~/components/info-fragments/DroneWarnings";
import ReadableDestination from "~/components/info-fragments/ReadableDestination";
import LabeledIcon from "~/components/generic/LabeledIcon";
import BatteryFragment from "~/components/info-fragments/BatteryFragment";

const statusQuery = graphql(`
    subscription DroneInfo($droneId: bigint!) {
        me: drones_by_pk(drone_id: $droneId) {
            telemetry: drone_telemetries(limit: 1, order_by: {timestamp: desc}) {
                # altitude
                battery
                heading
                # latitude
                # longitude
                velocity
            }
            flights(limit: 1, order_by: {flight_id: desc}) {
                order {
                    placed_at
                }
            }
        }
    }
`);
// TODO: placed_at is currently unused

const LoadingElem = <span>Drone has not produced enough data</span>;


/**
 * Card for detailed drone status page. Shows information on location, heading, battery, and speed.
 * Includes human-readable status & destination info. Has buttons to create a new flight.
 */
export default function DroneStatusCard() {
  const params = useParams();
  const droneInfo = createSubscription(statusQuery, {variables: {droneId: params.id}});
  const telemetry = () => droneInfo()?.me?.telemetry[0];
  const flight = () => droneInfo()?.me?.flights[0];
  createEffect(() => console.log(droneInfo()));

  return (
    <Card variant="outlined" sx={{display: "inline-block", padding: 2}}>
      {/* IDE shows errors, but trust, my type guards are foolproof */}
      {/* TODO: is there an async way of doing this?*/}
      <Show
        when={telemetry() && flight()}
        fallback={LoadingElem}
      >
        <Grid container spacing={2}>
          <Grid item>
            <Box sx={{textAlign: "center"}}>
              <img src="/drone.jpg" width="150px"  alt="Drone" />
            </Box>
            <LabeledIcon title="GPS location">
              <PlaceIcon sx={{ marginRight: "0.5em" }} />
              ({telemetry()!.latitude}, {telemetry()!.longitude})
            </LabeledIcon>
            <LabeledIcon title="Heading (degrees)">
              <CompassIcon sx={{ marginRight: "0.5em" }} />
              {telemetry()!.heading}°
            </LabeledIcon>
            <LabeledIcon title="Speed">
              <SpeedIcon sx={{ marginRight: "0.5em" }} />
              {telemetry()!.velocity} mph
            </LabeledIcon>
            <BatteryFragment id={params.id} />
          </Grid>
          <Grid item container direction="column" width={500}>
            <Grid item sx={{flexGrow: 99, textAlign: "center"}}>
              <Typography sx={{ fontSize: "2em", fontWeight: "bold" }}>Drone {params.id}</Typography>
              <DroneWarnings id={params.id} ok={<Typography>✅ Operational</Typography>} />
              <ReadableDestination id={params.id} />
            </Grid>
            <Grid item>
              <Stack spacing={2} direction="row" justifyContent="right">
                <Button variant="outlined">View Camera</Button>
                <Button variant="outlined">New Flight</Button>
              </Stack>
            </Grid>
          </Grid>
        </Grid>
      </Show>
    </Card>
  );
}

