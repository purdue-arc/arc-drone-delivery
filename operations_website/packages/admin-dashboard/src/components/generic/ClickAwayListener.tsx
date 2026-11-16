import {mergeProps, onCleanup, onMount, ParentProps} from "solid-js";

// TODO: contribute upstream
// MUI equivalent: packages/mui-base/src/ClickAwayListener/ClickAwayListener.tsx

type ClickAwayMouseEventHandler =
  | 'onClick'
  | 'onMouseDown'
  | 'onMouseUp'
  | 'onPointerDown'
  | 'onPointerUp';
type ClickAwayTouchEventHandler = 'onTouchStart' | 'onTouchEnd';

export interface ClickAwayListenerProps {
  /**
   * If `true`, the React tree is ignored and only the DOM tree is considered.
   * This prop changes how portaled elements are handled.
   * @default false
   */
  disableReactTree?: boolean;
  /**
   * The mouse event to listen to. You can disable the listener by providing `false`.
   * @default 'onClick'
   */
  mouseEvent?: ClickAwayMouseEventHandler | false;
  /**
   * Callback fired when a "click away" event is detected.
   */
  onClickAway: (event: MouseEvent | TouchEvent) => void;
  /**
   * The touch event to listen to. You can disable the listener by providing `false`.
   * @default 'onTouchEnd'
   */
  touchEvent?: ClickAwayTouchEventHandler | false;
}


function mapEventPropToEvent<EventHandlerName extends ClickAwayMouseEventHandler | ClickAwayTouchEventHandler>(
  eventProp: EventHandlerName,
): EventHandlerName extends `on${infer EventName}` ? Lowercase<EventName> : never {
  return eventProp.substring(2).toLowerCase() as any;
}

export default function ClickAwayListener(props: ParentProps & ClickAwayListenerProps) {
  let ref: HTMLDivElement;
  props = mergeProps({mouseEvent: "onClick", touchEvent: "onTouchEnd"} as const, props);

  const handleClick = (event: MouseEvent | TouchEvent) => {
    if (!ref.contains(event.target as Node))
      props.onClickAway(event);
  };

  onMount(() => {
    if (props.mouseEvent)
      document.addEventListener(mapEventPropToEvent(props.mouseEvent), handleClick);
    if (props.touchEvent)
      document.addEventListener(mapEventPropToEvent(props.touchEvent), handleClick);
  });

  onCleanup(() => {
    if (props.mouseEvent)
      document.removeEventListener(mapEventPropToEvent(props.mouseEvent), handleClick);
    if (props.touchEvent)
      document.removeEventListener(mapEventPropToEvent(props.touchEvent), handleClick);
  });

  return (
    <div ref={ref!}>
      {props.children}
    </div>
  );
}
