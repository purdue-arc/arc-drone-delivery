// @refresh reload
import {createResource, onMount, Suspense} from "solid-js";
import {Body, ErrorBoundary, FileRoutes, Head, Html, Meta, Routes, Scripts, Title} from "solid-start";
import "./root.css";
import SideNav from "~/components/SideNav";
import {Box, useTheme} from "@suid/material";
import {ApolloProvider} from "@merged/solid-apollo";
import gqlClient from "~/lib/gqlClient";
import Auth from "./routes/signin";
import {nhost} from "~/lib/nHost";

/** Render the frame common to all routes (navigation, graphql provider, auth) */
export default function Root() {
  const theme = useTheme();

  const [session, {mutate: setSession}] = createResource(
    () => nhost.auth.getSession(),
  );

  onMount(() => {
    nhost.auth.onAuthStateChanged((_event, session) => {
      setSession(session);
    });
  });

  return (
    <Html lang="en">
      <Head>
        <Title>Drone Delivery Admin Dashboard</Title>
        <Meta charset="utf-8" />
        <Meta name="viewport" content="width=device-width, initial-scale=1" />
      </Head>
      <Body>
        {/* Suspense docs: https://docs.solidjs.com/references/api-reference/control-flow/Suspense */}
        <Suspense>
          <ErrorBoundary>
            {!session() ? <Auth /> :
              <ApolloProvider client={gqlClient}>
                <SideNav />
                {/* TODO: This is JUST a few pixels off. https://mui.com/material-ui/customization/breakpoints/ might help? */}
                <Box sx={{ paddingLeft: theme.spacing(7) }}>
                  <Routes>
                    <FileRoutes />
                  </Routes>
                </Box>
              </ApolloProvider>
            }
          </ErrorBoundary>
        </Suspense>
        <Scripts />
      </Body>
    </Html>
  );
}
