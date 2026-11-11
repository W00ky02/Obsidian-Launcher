# Microsoft sign-in

## Current status

The Microsoft device-code sign-in flow is implemented, but Obsidian does not have its own registered and approved Microsoft client ID. Microsoft sign-in is therefore not ready to use in the released build. Installing this beta does not enable it.

Minecraft Services token sign-in and offline singleplayer remain available. You can also open the separate official launcher.

## Publisher setup

Before enabling Microsoft sign-in, the publisher needs to register its own public-client application, allow personal Microsoft accounts and the device-code flow, and obtain the required Minecraft Services API access. Do not use another launcher's client ID.

Obsidian uses the `XboxLive.SignIn` scope and exchanges the Microsoft session through Xbox, XSTS, and Minecraft Services before retrieving the Java profile. No client secret is used by this desktop flow.

An approved public application ID can be entered under Settings, in the optional developer settings for Microsoft sign-in. That field is for a public client ID, not a password, access token, or client secret.

Check the current requirements with Microsoft and Minecraft before registering an application:

- [Register an application](https://learn.microsoft.com/en-us/entra/identity-platform/quickstart-register-app)
- [Microsoft device authorization flow](https://learn.microsoft.com/en-us/entra/identity-platform/v2-oauth2-device-code)
- [Minecraft Help](https://help.minecraft.net/)

## Session handling

Sign-in happens in the browser using the code shown by Obsidian. The account must have an available Minecraft Java profile. A successful flow retrieves the profile name and UUID automatically.

OAuth, Xbox, and Minecraft credentials are held in memory. Obsidian does not ask for a Microsoft password, save a token cache, or persist refresh tokens. Sign out, close the launcher, or let the session expire to end the session. Network errors do not show raw token responses. TLS checks stay enabled, and authentication requests do not follow HTTP redirects.

The flow has been tested against a local test server. Real Microsoft and Minecraft sign-in has not been verified without an approved client ID.
