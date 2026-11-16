import {Box, Button, Stack} from "@suid/material";
import {useNavigate} from "@solidjs/router";
import DroneWarnings from "~/components/info-fragments/DroneWarnings";
import {Suspense} from "solid-js";
import ReadableDestination from "~/components/info-fragments/ReadableDestination";
import BatteryFragment from "~/components/info-fragments/BatteryFragment";

/**
 * Similar to DroneStatusCard, but only show readable critical information, along with buttons to view details or new flight
 * @param props.id drone id to show info on
 * @param props.onStartDrawingPath Call when "New Flight" button pressed
 */
export default function DroneTooltipContents(props: {id: number, onStartDrawingPath: () => void}) {
  const navigate = useNavigate();

  return (
    <Box p={2} width={300}>
        <Stack sx={{flexDirection: "column", gap: 2, alignItems: "center"}}>
          <Suspense>
            <DroneWarnings id={props.id} />
            <ReadableDestination id={props.id} />
            <BatteryFragment id={props.id} />
          </Suspense>
          <Stack spacing={2} direction="row" justifyContent="space-evenly">
            <Button variant="outlined" onClick={props.onStartDrawingPath}>New Flight</Button>
            <Button variant="contained" onClick={() => navigate("/drone/" + props.id)}>View Details</Button>
          </Stack>
        </Stack>
    </Box>
  );
}
