import {
  Button,
  Dialog,
  DialogActions,
  DialogContent,
  DialogTitle,
  FormControl,
  InputAdornment,
  InputLabel,
  MenuItem,
  OutlinedInput,
  Select,
  Stack,
  TextField
} from "@suid/material";
import {SelectChangeEvent} from "@suid/material/Select";
import {createSignal, Index} from "solid-js";
import {graphql} from "~/gql";
import {createMutation, createQuery} from "@merged/solid-apollo";
import {nhost} from "~/lib/nHost";
import {type Cartographic, Math as CesiumMath} from "cesium";

const vendorsQuery = graphql(`
    query QueryVendors {
        vendors {
            id
            name
            lat
            long
        }
    }
`);

const createOrderMutation = graphql(`
    mutation CreateOrderMutation($dest_lat: float8, $dest_long: float8, $pickup_lat: float8, $pickup_long: float8, $food_items: [String!], $placed_at: bigint, $price: float8, $user_id: uuid, $vendor_id: bigint) {
        insert_orders_one(object: {dest_lat: $dest_lat, dest_long: $dest_long, vendor_id: $vendor_id, user_id: $user_id, placed_at: $placed_at, food_items: $food_items, pickup_lat: $pickup_lat, pickup_long: $pickup_long, price: $price}) {
            order_id
            vendor {
                name
            }
        }
    }
`);

export interface OrderSummary {
  orderId: number,
  vendorName: string,
}

/**
 *
 * @param props.setOrder Function to call (likely a Solid signal setter) when a new order has been created.
 * @param props.ref Function passed to parent that will open the dialog. Need to pass regular `let` variable of type `void function` despite typing.
 * @param props.dst Destination for order. Last point in path is a reasonable choice
 * @constructor
 */
export default function NewOrderDialog(props: {setOrder: (o: OrderSummary) => void, ref?: (openDialog: () => void) => void, dst: Cartographic}) {
  const [isOpen, setIsOpen] = createSignal(false);
  const handleClose = () => setIsOpen(false);
  // Solid complains if I don't do this
  const [selectedVendorName, setSelectedVendorName] = createSignal("");
  const vendors = createQuery(vendorsQuery);
  const [submitOrder, orderResult] = createMutation(createOrderMutation);

  const handleChange = (event: SelectChangeEvent) => {
    setSelectedVendorName(event.target.value);
  };

  function openDialog() {
    setIsOpen(true);
    setSelectedVendorName("");
  }
  // Passing from child to parent as ref kinda hacky, but alternative was signals
  props.ref?.(openDialog);

  function handleSubmit(event: SubmitEvent) {
    event.preventDefault();
    const formData = new FormData(event.currentTarget as HTMLFormElement);
    // Form validation ensures `vendors` exists
    const vendor = vendors()!.vendors[Number.parseInt(formData.get("vendor") as string)];
    submitOrder({variables: {
      vendor_id: vendor.id,
      pickup_lat: vendor.lat,
      pickup_long: vendor.long,
      price: Number.parseInt(formData.get("cost") as string),
      food_items: (formData.get("items") as string).split("\n"),
      placed_at: new Date().getTime(),
      user_id: nhost.auth.getSession()!.user.id,
      dest_lat: CesiumMath.toDegrees(props.dst.latitude),
      dest_long: CesiumMath.toDegrees(props.dst.longitude),
    }}).then(({insert_orders_one: res}) =>
      props.setOrder({
        orderId: res!.order_id,
        vendorName: res!.vendor.name,
      } satisfies OrderSummary)
    );
    handleClose();
  }

  return (
    <Dialog
      open={isOpen()}
      onClose={handleClose}
      component="form"  // Well, this doesn't work TODO: fix upstream?
      PaperProps={{
        component: "form",
      }}
    >
      <form onSubmit={handleSubmit}>
        <DialogTitle>New Order</DialogTitle>
        <DialogContent>
          <Stack spacing={2}>
            <FormControl>
              <InputLabel>Vendor</InputLabel>
              <Select
                label="Vendor"
                name="vendor"
                required
                value={selectedVendorName()}
                onChange={handleChange}
              >
                <Index each={vendors()?.vendors ?? []}>{(vendor, idx) =>
                  <MenuItem value={idx}>{vendor().name}</MenuItem>
                }</Index>
              </Select>
            </FormControl>
            <TextField
              label="Food items"
              name="items"
              multiline
              required
              rows={4}
            />
            <FormControl>
              <InputLabel>Cost</InputLabel>
              <OutlinedInput
                required
                margin="dense"
                name="cost"
                label="Cost"
                type="number"
                startAdornment={<InputAdornment position="start">$</InputAdornment>}
              />
            </FormControl>
          </Stack>
        </DialogContent>
        <DialogActions>
          <Button onClick={handleClose}>Cancel</Button>
          <Button type="submit">Create</Button>
        </DialogActions>
      </form>
    </Dialog>
  )
}
