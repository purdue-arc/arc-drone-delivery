import {styled, Typography} from "@suid/material";

/** Typography element with vertical centering. Useful for centering icons with text. Put both icon and label as direct children */
const LabeledIcon = styled(Typography)({
  display: "flex",
  alignItems: "center",
  lineHeight: 0,
});

export default LabeledIcon;
