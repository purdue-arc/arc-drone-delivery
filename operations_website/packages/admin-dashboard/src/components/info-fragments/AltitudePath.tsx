// import {createEffect, createSignal, onMount} from "solid-js";
// import {graphql} from "~/gql";
// import {createSubscription} from "@merged/solid-apollo";
// import {Cartographic, Math as CesiumMath} from "cesium";
// import {Chart, registerables} from "chart.js";

// Chart.register(...registerables);

// // write a graphql query to get the altitude of the current drone id from the drone_telemetry databse
// const altitudeQuery = graphql(`
// subscription AltitudeInfo($droneId: bigint!) {
//     info: drone_telemetry(where: {drone_id: {_eq: $droneId}}, order_by: {timestamp: desc}) {
//       altitude
//       latitude
//       longitude
//     }
//   }
  
// `);


// /**
//  * function that shows a 2d representation of the altitude of the drone over time
//  * @param props.id id of the drone
//  */
// export default function AltitudePathGraph(props: {id: number, points: Cartographic[]} ) {
//     let chart: Chart | null = null;
//     const droneAltitudeInfo = createSubscription(altitudeQuery, {variables: {droneId: props.id}});
//     createEffect(() => console.log(droneAltitudeInfo()));
//     // the altitude should have a value of 0 in index 0
//     const [altitudes, setAltitudes] = createSignal<number[]>([0]);
//     createEffect(() => {
//         const newAltitudes = props.points.map(pt => pt.height);
//         newAltitudes.unshift(0);
//         setAltitudes(newAltitudes);
//         // console.log("altitudes");
//         // console.log(newAltitudes);
//     });
//     // calculate the distance between two latitude and longitude points using the haversine formula
//     function calculateLatLongDistance(lat1: number, lon1: number, lat2: number, lon2: number) {
//         const p = 0.017453292519943295;    // Math.PI / 180
//         const c = Math.cos;
//         const a = 0.5 - c((lat2 - lat1) * p)/2 +
//                   c(lat1 * p) * c(lat2 * p) *
//                   (1 - c((lon2 - lon1) * p))/2;

//         return 12742 * Math.asin(Math.sqrt(a)); // 2 * R; R = 6371 km
//       }
//     // calculate the distance between each point and add it to a list and upadate it whenever props.points changes
//     const [distances, setDistances] = createSignal<number[]>([0]);
//     createEffect(() => {
//         const newDistances = [];
//         for (let i = 0; i < props.points.length - 1; i++) {
//             // const point1 = Cartographic.fromCartesian(props.points[i]);
//             // const point2 = Cartographic.fromCartesian(props.points[i+1]);
//             const lat1 = CesiumMath.toDegrees(props.points[i].latitude);
//             const lon1 = CesiumMath.toDegrees(props.points[i].longitude);
//             const lat2 = CesiumMath.toDegrees(props.points[i + 1].latitude);
//             const lon2 = CesiumMath.toDegrees(props.points[i+1].longitude);
//             console.log("lat lon");
//             console.log(lat1, lon1, lat2, lon2);

//             newDistances.push(calculateLatLongDistance(lat1, lon1, lat2, lon2));
//         }
//         newDistances.unshift(0);
//         setDistances(newDistances);
//         // console.log("distances");
//         // console.log(newDistances);
//     });
//     onMount(() => {
//         const ctx = document.getElementById("myChart") as HTMLCanvasElement;
//     chart = new Chart(ctx, {
//         type: "line",
//         data: {
//             labels: distances(),
//             datasets: [{
//                 label: "Altitude",
//                 data: altitudes(), // replace with your altitudes array
//                 fill: false,
//                 borderColor: "rgb(75, 192, 192)",
//                 tension: 0.1,
//             }],
//         },
//         options: {
//             responsive: true,
//             maintainAspectRatio: false,
//         },
//     });
//     });
//     createEffect(() => {
//         if (chart) {
//             chart.data.labels = distances();
//             chart.data.datasets[0].data = altitudes();
//             chart.update();
//         }
//     });


//     return (
//         <canvas id="myChart" />
//     );
// }
