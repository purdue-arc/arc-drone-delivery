import "./Tooltip.css";
import {type JSX} from "solid-js";

/**
 * Container for tooltips (styling only)
 * @param props.children contents
 * @param props.x fixed x position
 * @param props.y fixed y position
 * @see https://www.w3schools.com/css/css_tooltip.asp
 */
export default function Tooltip(props: { children: JSX.Element, x: number, y: number }) {
  return (
    <div class="tooltip" style={{top: props.y + "px", left: props.x + "px"}}>
      <span class="tooltiptext">{props.children}</span>
    </div>
  );
}
