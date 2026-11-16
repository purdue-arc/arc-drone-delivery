import {Box, Grid} from "@suid/material";
import DroneStatusCard from "~/components/cards/DroneStatusCard";
import BatteryReleaseChartCard from "~/components/cards/VoltageTimeGraphCard";
import OrderHistoryCard from "~/components/cards/OrderHistoryCard";

/** Show all the cards relevant to the drone specified in url paramete */
export default function DroneStatus() {
  // TODO: direct to error page if drone ID doesn't exist

  return (
    <Grid container spacing={2} p={2} columns={2}>
      <Grid item xs={2}>
        <Box justifyContent="center" width="100%" sx={{display: "flex"}}>
          <DroneStatusCard />
        </Box>
      </Grid>
      <Grid item>
        <BatteryReleaseChartCard droneId={0} />
      </Grid>
      <Grid item>
        <OrderHistoryCard />
      </Grid>
    </Grid>
  );
}



