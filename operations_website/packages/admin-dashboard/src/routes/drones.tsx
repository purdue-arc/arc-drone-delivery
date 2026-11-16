import {Box, Paper, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, Typography} from "@suid/material";
import {For} from "solid-js";
import {graphql} from "~/gql";
import {createSubscription} from "@merged/solid-apollo";


const SUBSCRIPTION = graphql(`
subscription GetDrones {
  drone_telemetry(distinct_on: drone_id, order_by: {timestamp: desc, drone_id: asc}) {
    drone_id
    has_package
    # altitude
    battery
    timestamp
  }
}`);

/**
 * Page with table of all drone info
 */
export default function Drones() {

  let data = createSubscription(SUBSCRIPTION);

  return (
    <Box padding={2}>
      <Typography variant="h3">All Drones</Typography>
      <TableContainer sx={{ borderRadius: 2, border: 1, borderColor: "rgba(224.4, 224.4, 224.4, 1)" }} component={Paper}>
        <Table sx={{ minWidth: 650 }} aria-label="simple table">
          <TableHead>
            <TableRow>
              <For each={Object.keys(data()?.drone_telemetry[0] || {})}>
                {(columnName) => <TableCell>{columnName}</TableCell>}
              </For>
            </TableRow>
          </TableHead>
          <TableBody>
            <For each={data()?.drone_telemetry || []}>{(row) => (
              <TableRow sx={{ "&:last-child td, &:last-child th": { border: 0 } }}>
                <For each={Object.values(row)}>
                  {(cell) => (
                    // toString() needed to turn boolean into string
                    <TableCell>{cell.toString()}</TableCell>
                  )}
                </For>
              </TableRow>
            )}
            </For>
          </TableBody>
        </Table>
      </TableContainer>
    </Box>
  );
}
