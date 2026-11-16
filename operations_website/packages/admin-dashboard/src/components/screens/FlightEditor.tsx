import {Box, Button, IconButton, MenuItem, MenuList, Paper, Popper, Stack, Typography} from "@suid/material";
import ClickAwayListener from "~/components/generic/ClickAwayListener";
import ArrowDropUpIcon from "@suid/icons-material/ArrowDropUp";
import SearchIcon from "@suid/icons-material/Search";
import {createSignal, For, Show} from "solid-js";
import type PathController from "~/lib/cesium/PathController";
import {type Cartographic, Math as CesiumMath} from "cesium";
import {graphql} from "~/gql";
import {createMutation} from "@merged/solid-apollo";
import NewOrderDialog, {OrderSummary} from "~/components/NewOrderDialog";
import NoteAddIcon from "@suid/icons-material/NoteAdd";

const submitFlightMutation = graphql(`
mutation InsertFlights($drone_id: bigint, $order_id: bigint, $route: [String!] = "") {
    insert_flights(objects: {drone_id: $drone_id, order_id: $order_id, route: $route}) {
        returning {
            flight_id
        }
    }
}
`);


/**
 * Sidebar that shows when editing a drone path on index.tsx page
 * @param props.points gps positions of sky waypoints
 * @param props.pathController reference to the active PathController
 * @param props.close Function to call to close component
 */
export default function FlightEditor(props: { points: Cartographic[], pathController: PathController, close: () => void }) {
  const [isOpen, setIsOpen] = createSignal(false);
  const [attachedOrder, setAttachedOrder] = createSignal<OrderSummary>();
  const addFlight = createMutation(submitFlightMutation)[0];
  let anchorRef: HTMLButtonElement | undefined;
  // Fn from `NewOrderDialog` that opens a modal
  let createNewOrder!: () => void;

  const options = [
    {name: "Local", onClick: () => props.pathController.simulateLocal()},
    {name: "Database", onClick: () => props.pathController.simulateDatabase()},
    {name: "Stop", onClick: () => props.pathController.endSimulation()},
  ];

  function submitFlight() {
    if (props.points.length < 2) {
      alert("Need at least 2 points to submit a flight");
      return;
    }
    addFlight({variables: {
      drone_id: props.pathController.drone.id,
      route: props.points.map(pt => JSON.stringify([CesiumMath.toDegrees(pt.latitude), CesiumMath.toDegrees(pt.longitude), pt.height])),
      order_id: attachedOrder()?.orderId,
    }}).then(props.close).catch(alert);
  }

  return (
    <Box padding={2}>
      <Typography variant="h3">Edit Flight</Typography>
      <Typography>
        Controls:<br />
        Left click: place waypoint<br />
        r/f: raise/lower altitude of waypoint<br />
        Right click: Finish drawing path
      </Typography>

      <fieldset>
        <legend>Order Id</legend>
        <Show when={attachedOrder()} fallback={<>(none selected)</>}>
          {JSON.stringify(attachedOrder())}
        </Show>
        <IconButton onClick={() => createNewOrder()} title="Create new order">
          <NoteAddIcon />
        </IconButton>
        <NewOrderDialog setOrder={setAttachedOrder} ref={createNewOrder} dst={props.points.at(-1)!} />
        <IconButton onClick={() => console.log("select")} title="Select existing order">
          <SearchIcon />
        </IconButton>
      </fieldset>

      {/* <Typography variant="h4">Waypoints</Typography>

      <Index each={props.points}>{(point, i) => <li>{point().toString()}</li>}</Index> */}

      <Stack spacing={2} direction="row" justifyContent="right">
        <Button variant="outlined" ref={anchorRef} onClick={() => setIsOpen(prev => !prev)}>
          Simulate <ArrowDropUpIcon />
        </Button>
        <Button variant="contained" color="primary" onClick={submitFlight}>
          Submit Flight
        </Button>
      </Stack>

      {/* Dropdown for simulate button */}
      <Popper
        open={isOpen()}
        anchorEl={anchorRef}
      >
        <Paper>
          <ClickAwayListener onClickAway={() => setIsOpen(false)}>
            <MenuList>
              <For each={options}>{({name, onClick}) =>
                <MenuItem
                  onClick={onClick}
                >
                  {name}
                </MenuItem>
              }</For>
            </MenuList>
          </ClickAwayListener>
        </Paper>
      </Popper>

    </Box>
  );
}
