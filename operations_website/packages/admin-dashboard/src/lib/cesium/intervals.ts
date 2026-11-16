import * as Cesium from "cesium";

export const foreverInterval = new Cesium.TimeIntervalCollection([
  new Cesium.TimeInterval({
    start: new Cesium.JulianDate(),
    // Necessary to view path. INFINITY & MAX_INTEGER don't work
    stop: Cesium.JulianDate.addDays(
      Cesium.JulianDate.now(),
      365,
      new Cesium.JulianDate(),
    ),
  }),
]);
