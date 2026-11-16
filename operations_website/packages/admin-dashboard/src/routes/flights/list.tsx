import {
  Box,
  Button,
  Paper,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  Typography,
} from "@suid/material";
import {For} from "solid-js";

/**
 * TODO: delete this
 * @param name
 * @param calories
 * @param fat
 * @param carbs
 * @param protein
 */
function createData(
  name: string,
  calories: number,
  fat: number,
  carbs: number,
  protein: number,
) {
  return { name, calories, fat, carbs, protein };
}

const rows = [
  createData("Frozen yoghurt", 159, 6.0, 24, 4.0),
  createData("Ice cream sandwich", 237, 9.0, 37, 4.3),
  createData("Eclair", 262, 16.0, 24, 6.0),
  createData("Cupcake", 305, 3.7, 67, 4.3),
  createData("Gingerbread", 356, 16.0, 49, 3.9),
];


/** TODO */
export default function Flights() {
  return (
    <Box padding={2}>


    <Typography variant="h3">All Flights</Typography>
    <Box sx={{display: "flex", justifyContent: "flex-end", mb: 2}}>
        <Button variant="contained" color="primary" href="/flights/create" sx={{marginLeft: "auto"}}>
            Create Flight
        </Button>
    </Box>
      {/*TODO: no clue where borderColor came from*/}
      {/* Adapted from: https://mui.com/material-ui/react-table/#basic-table */}
      <TableContainer sx={{borderRadius: 2, border: 1, borderColor: "rgba(224.4, 224.4, 224.4, 1)"}} component={Paper}>
        <Table sx={{ minWidth: 650 }} aria-label="simple table">
          <TableHead>
            <TableRow>
              <TableCell>Dessert (100g serving)</TableCell>
              <TableCell align="right">Calories</TableCell>
              <TableCell align="right">Fat&nbsp;(g)</TableCell>
              <TableCell align="right">Carbs&nbsp;(g)</TableCell>
              <TableCell align="right">Protein&nbsp;(g)</TableCell>
            </TableRow>
          </TableHead>
          <TableBody>
            <For each={rows}>{(row, i) =>
              <TableRow
                sx={{"&:last-child td, &:last-child th": {border: 0}}}
              >
                <TableCell component="th" scope="row">
                  {row.name}
                </TableCell>
                <TableCell align="right">{row.calories}</TableCell>
                <TableCell align="right">{row.fat}</TableCell>
                <TableCell align="right">{row.carbs}</TableCell>
                <TableCell align="right">{row.protein}</TableCell>
              </TableRow>
            }</For>
          </TableBody>
        </Table>
      </TableContainer>
    </Box>
  );
}
