import {createSignal} from "solid-js";
import {Line} from "solid-chartjs";
import {Card} from "@suid/material";

/**
 * Card for detailed drone status page. Shows the specified drone's battery level over time as a line plot
 * @param props.droneId TODO: use `const params = useParams();` and `params.id` instead
 */
export default function BatteryReleaseChartCard(props: {droneId: number}) {
    const [data] = createSignal({
        labels: Array.from({length: 10}, (_, i) => i + 1), //times in seconds
        datasets: [
            {
                label: "Battery Voltage",
                data: Array.from({length: 10}, () => Math.random() * 10), //Random voltage values
                fill: false,
                borderColor: "rgb(75, 192, 192)",
                tenson: 0.1,
            },
        ],
    });
    return (
        <Card variant="outlined">
            <Line data={data()} options = {{
                type: "line",
                responsive: true,
                scales: {
                    x: {
                        title: {
                            display: true,
                            text: "Time (seconds)",
                        },
                    },
                    y: {
                        title: {
                            display: true,
                            text: "Voltage (V)",
                        },
                    },
                },
            }} />
        </Card>
    );
}


// import { onMount } from 'solid-js'
// import { Chart, Title, Tooltip, Legend, Colors } from 'chart.js'
// import { Line } from 'solid-chartjs'
// export default function BatteryReleaseChartCard(props: {droneId: number}) {
//         /**
//          * You must register optional elements before using the chart,
//          * otherwise you will have the most primitive UI
//          */
//         onMount(() => {
//             Chart.register(Title, Tooltip, Legend, Colors)
//         })

//         const chartData = {
//             labels: ['January', 'February', 'March', 'April', 'May'],
//             datasets: [
//                 {
//                     label: 'Sales',
//                     data: [50, 60, 70, 80, 90],
//                 },
//             ],
//         }
//         const chartOptions = {
//             responsive: true,
//             maintainAspectRatio: false,
//         }

//         return (
//             <div>
//                 <Line data={chartData} options={chartOptions} width={500} height={500} />
//             </div>
//         )

// }
