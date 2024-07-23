/*
 * OpenKneeboard API - ISC License
 *
 * Copyright (C) 2022-2023 Fred Emmott <fred@fredemmott.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * ----------------------------------------------------------------
 * ----- THE ABOVE LICENSE ONLY APPLIES TO THIS SPECIFIC FILE -----
 * ----------------------------------------------------------------
 *
 * The majority of OpenKneeboard is under a different license; check specific
 * files for more information.
 */

declare interface OpenKneeboardAPIError extends Error {
    apiMethodName: string;
}

declare namespace OpenKneeboard {
    export interface VersionComponents {
        Major: number;
        Minor: number;
        Patch: number;
        Build: number;
    }

    export interface Version {
        Components: VersionComponents;
        HumanReadable: string;
        IsGitHubActionsBuild: boolean;
        IsTaggedVersion: boolean;
        IsStableRelease: boolean;
    }

    export interface API extends EventTarget {
        GetVersion(): Promise<Version>;

        SetPreferredPixelSize(width: number, height: number): Promise<any>;

        EnableExperimentalFeature(name: string, version: number): Promise<any>;
        EnableExperimentalFeatures(features: [{ name: string, version: number }]): Promise<any>;
    }
}

declare var OpenKneeboard: OpenKneeboard.API;