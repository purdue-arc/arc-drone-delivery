# Drone Delivery Website

This is a [`solid-start`](https://start.solidjs.com) website intended to handle drone operations including:

* Manually contolling the drone
* Operating a network of drones
* Drone delivery operations
* Monitoring drone operations

It uses [CesiumJS](https://cesium.com/learn/cesiumjs-learn/) as a geospatial rendering library.

![image](docs/flight-editor.png)

## Developing

### 1. Choose the environment

#### GitHub Codespace

This is the easiest method as it involves installing nothing on your computer. Make sure you have the GitHub student developer account on your account. This will give you free access to GitHub Codespaces. Next, go to https://github.com/purdue-arc/drone-delivery-website and click the `Code` button and then `Codespaces` and launch the repository.

#### Docker container locally

This is the preferred method if you want to run it locally.

The repository contains a [dev container](https://code.visualstudio.com/docs/devcontainers/containers) configuration and a docker-compose file for you to easily get a container running.

If you are running Windows, make sure to clone this repository in your WSL distro as it will run faster than if you clone it on Windows.

If you are using VSCode, make sure you have the dev containers extension installed.

After adding the extension, run the  "Reopen folder in container" command by searching for it after pressing `Ctrl/Cmd+Shift+P`.

If you are not using VSCode, you should start the container by running `docker compose up` in the `.devcontainer` folder and connect to the repository inside the container.

You should connect to the repository inside the container so that you are using the NodeJS and NPM inside the container to make changes to the repository. Although you could install new Node modules with your local installation, we want to avoid that as it could break things.

#### Locally

If the previous options don't work for you, just install Node.JS on your system.


### 2. Add Access Tokens

CesiumJS requires that you have an access token to be able to access geospatial elements through their service Cesium Ion such as satelite imagery. We don't want to leak that token so that other people can use our free credits, so we store it in an environment variable in a `.env.local` file that the [Vite reads](https://vitejs.dev/guide/env-and-mode.html) from and that is not tracked by git (see `.gitignore`). Copy `.env.local.sample` to `.env.local` by running

```
cp .env.local.sample .env.local
``` 

and then fill in the access token variable (no quotes surrounding the token).

**Where to find**
- Hasura admin secret: Nhost project -> hasura tab
- Nhost urls: Nhost project ->  `settings/environment-variables`
- [Cesium token](https://ion.cesium.com/tokens)

### 3. Run the app

Run `npm install` to get the latest packages and `npm run dev` to start the app.

**App will not run under other package managers besides npm.**

<details>
<summary>Why?</summary>

- pnpm: Graphql codegen types are incorrect (reporting generic object)
- yarn: React is not defined (likely from migrating `package-lock.json` to `yarn.lock`. Oddly, it worked until I applied monorepo change)
- npm: Initially, wasn't able to find Cesium assets, hence I tried pnpm & yarn. Workaround: postinstall script symlinks
</details>

## Building

Solid apps are built with _adapters_, which optimise your project for deployment to different environments.

By default, `npm run build` will generate a Node app that you can run with `npm start`. To use a different adapter, add it to the `devDependencies` in `package.json` and specify in your `vite.config.js`.

## Tech Stack Resources
1. [Solid.js](https://www.solidjs.com/tutorial/): Reactive ui library
   - [Testing](https://docs.solidjs.com/guides/how-to-guides/testing-in-solid/vitest) or maybe cypress
2. Solid Start (Web framework using solid.js)
3. [Material UI](https://mui.com/material-ui/react-autocomplete/): design framework by google & component library (but for React)
   - [SolidJS User Interface Design (SUID)](https://suid.io/): A port of Material-UI (MUI) built with SolidJS. Identical to Material UI, reference above docs
   - [Material icons](https://mui.com/material-ui/material-icons/)
   - [Styling](https://mui.com/system/properties/) & [sx prop](https://mui.com/system/getting-started/the-sx-prop/)
4. [Cesium](https://sandcastle.cesium.com/): 3D map rendering engine
5. [Nhost](https://nhost.io/)
   - Postgres
   - Auth
   - Hasura
   - GraphQL
6. Vite: Build tool

## Monorepo architecture
- admin-dashboard: our frontend website
  - validator (perform client-side validation before we make Hasura Action requests)
  - Nhost Auth 
    - Hasura GraphQL endpoint
      - Postgres Query
      - Hasura Action
         - dd-api.purduearc.com
