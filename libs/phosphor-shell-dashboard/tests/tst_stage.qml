// SPDX-FileCopyrightText: 2026 fuddlesworth
// SPDX-License-Identifier: LGPL-2.1-or-later
import QtQuick
import QtTest
import Phosphor.Dashboard

TestCase {
    id: testCase
    name: "StageOverview"
    when: windowShown
    visible: true
    width: 1440
    height: 900
    Component {
        id: mapComponent
        QtObject {
            property var windows: [
                {
                    id: "a",
                    windowId: "a",
                    title: "Editor",
                    focused: true
                },
                {
                    id: "b",
                    windowId: "b",
                    title: "Browser"
                }
            ]
            property var cells: []
            property rect workArea: Qt.rect(0, 76, 1440, 824)
            property int mode: 1
            property var lens: ({})
            property int overflowLeft: 0
            property int overflowRight: 0
            property int stripExtentPx: 0
            property int scrollDelta: 0
            property real aspect: 1.6
            property var menuModel: []
            property string activated: ""
            signal changed
            function refreshMenu() {
            }
            function activateNavigationWindow(id) {
                activated = id;
            }
            function scrollViewByPx(px) {
                scrollDelta += px;
            }
        }
    }
    Component {
        id: stageComponent
        StageOverview {
            width: testCase.width
            height: testCase.height
        }
    }
    Component {
        id: workspacesComponent
        QtObject {
            property int count: 2
            property int activeIndex: 1
            property string activeName: "Develop"
            property var model: [
                {
                    name: "Develop",
                    workspaceId: "develop",
                    isActive: activeIndex === 1
                },
                {
                    name: "Build",
                    workspaceId: "build",
                    isActive: activeIndex === 2
                }
            ]
        }
    }
    function test_workspaceCardsResolveAgainAfterDesktopSwitch() {
        const live = createTemporaryObject(mapComponent, testCase);
        const first = createTemporaryObject(mapComponent, testCase);
        const second = createTemporaryObject(mapComponent, testCase);
        const workspaces = createTemporaryObject(workspacesComponent, testCase);
        // Plain JS state mirrors the C++ factory: it cannot establish a QML
        // dependency on the active desktop inside the mapFor callback.
        const context = {
            current: 0
        };
        const stage = createTemporaryObject(stageComponent, testCase, {
            workspaces: workspaces,
            mapFor: index => index === context.current ? live : (index === 0 ? first : second),
            open: true
        });
        waitForRendering(stage);
        compare(findChild(stage, "stage-workspace-0").map, live);
        compare(findChild(stage, "stage-workspace-1").map, second);
        context.current = 1;
        workspaces.activeIndex = 2;
        compare(findChild(stage, "stage-workspace-0").map, first);
        compare(findChild(stage, "stage-workspace-1").map, live);
    }
    function test_selectionIsSeparateFromActivation() {
        const map = createTemporaryObject(mapComponent, testCase);
        const stage = createTemporaryObject(stageComponent, testCase, {
            mapFor: () => map,
            open: true
        });
        waitForRendering(stage);
        stage.forceActiveFocus();
        keyClick(Qt.Key_Right);
        compare(stage.selectedId, "b");
        compare(map.activated, "");
        keyClick(Qt.Key_Return);
        compare(map.activated, "b");
    }
    function test_scrollingIncludesClippedHeadersAndPreservesAspectRatio() {
        const map = createTemporaryObject(mapComponent, testCase);
        map.mode = 2;
        map.workArea = Qt.rect(0, 76, 1908, 1532);
        // The top card is partly underneath the desktop bar. Another parked
        // window has a huge offscreen position that must not affect the fit.
        map.windows = [
            {
                windowId: "top",
                x: 44 / 1908,
                y: -76 / 1532,
                w: 1820 / 1908,
                h: 552 / 1532
            },
            {
                windowId: "bottom",
                x: 44 / 1908,
                y: 492 / 1532,
                w: 1820 / 1908,
                h: 924 / 1532
            },
            {
                windowId: "parked",
                offscreen: true,
                x: 0,
                y: 20,
                w: 1,
                h: 1
            }
        ];
        const stage = createTemporaryObject(stageComponent, testCase, {
            width: 1908,
            height: 1608,
            mapFor: () => map
        });
        const top = stage.windowRect(map.windows[0]);
        const bottom = stage.windowRect(map.windows[1]);
        verify(top.y >= stage.previewRect.y);
        verify(bottom.y + bottom.height <= stage.previewRect.y + stage.previewRect.height + 0.001);
        verify(bottom.y > top.y + top.height);
        fuzzyCompare(top.width / 1820, top.height / 552, 0.001);
        fuzzyCompare(bottom.width / 1820, bottom.height / 924, 0.001);
        verify(top.height > 100);
        // The same edge condition on a horizontal strip retains both headers.
        map.windows = [
            {
                windowId: "left",
                x: -0.1,
                y: 0,
                w: 0.6,
                h: 1
            },
            {
                windowId: "right",
                x: 0.51,
                y: 0,
                w: 0.6,
                h: 1
            }
        ];
        const left = stage.windowRect(map.windows[0]), right = stage.windowRect(map.windows[1]);
        verify(left.x >= stage.previewRect.x - 0.001);
        verify(right.x + right.width <= stage.previewRect.x + stage.previewRect.width + 0.001);
    }
    function test_scrollingWindowStripUsesBoundedTitleCards() {
        const map = createTemporaryObject(mapComponent, testCase);
        map.mode = 2;
        map.windows = [
            {
                windowId: "long-title",
                appId: "org.kde.konsole",
                title: "~/Projects/phosphor-works/.worktrees/shell-design/libs/phosphor-shell-dashboard - fish — Konsole"
            }
        ];
        const stage = createTemporaryObject(stageComponent, testCase, {
            mapFor: () => map,
            open: true
        });
        waitForRendering(stage);
        const strip = findChild(stage, "stage-window-list");
        verify(strip.visible);
        const card = strip.itemAtIndex(0);
        verify(card);
        verify(card.row);
        verify(card.width <= 220);
        verify(card.contentItem.clip);
        const footer = findChild(stage, "stage-footer");
        verify(footer.y >= strip.y + strip.height + 12);
    }
    function test_scrollingWindowStripFollowsSelection() {
        const map = createTemporaryObject(mapComponent, testCase);
        map.mode = 2;
        map.windows = [];
        for (let i = 0; i < 12; ++i) {
            map.windows.push({
                windowId: "window-" + i,
                appId: "org.kde.konsole",
                title: "Window " + i,
                x: i / 12,
                y: 0,
                w: 0.2,
                h: 1
            });
        }
        const stage = createTemporaryObject(stageComponent, testCase, {
            mapFor: () => map,
            open: true
        });
        waitForRendering(stage);
        const strip = findChild(stage, "stage-window-list");
        stage.select(11);
        tryCompare(strip, "contentX", strip.contentWidth - strip.width, 1000);
        compare(stage.selectedId, "window-11");
    }
    function test_scrollingSelectionPansTheLiveLensToAnOffscreenWindow() {
        const map = createTemporaryObject(mapComponent, testCase);
        map.mode = 2;
        map.stripExtentPx = 1000;
        map.lens = {
            x: 0,
            y: 0,
            w: 0.25,
            h: 1,
            vertical: false
        };
        map.windows = [
            {
                windowId: "visible",
                appId: "org.kde.konsole",
                stripT: 0.05,
                x: 0.05,
                y: 0,
                w: 0.2,
                h: 1,
                offscreen: false
            },
            {
                windowId: "offscreen",
                appId: "org.kde.konsole",
                stripT: 0.82,
                x: 3.28,
                y: 0,
                w: 0.2,
                h: 1,
                offscreen: true
            }
        ];
        const stage = createTemporaryObject(stageComponent, testCase, {
            mapFor: () => map,
            open: true
        });
        waitForRendering(stage);
        stage.select(1);
        tryCompare(map, "scrollDelta", 695, 1000);
        compare(stage.selectedId, "offscreen");
    }
    function test_desktopCanvasMapsToPreviewAcrossOutputShapes() {
        const map = createTemporaryObject(mapComponent, testCase);
        const stage = createTemporaryObject(stageComponent, testCase, {
            mapFor: () => map
        });
        for (const size of [[1440, 900], [800, 600], [900, 1440]]) {
            stage.width = size[0];
            stage.height = size[1];
            map.workArea = Qt.rect(0, 76, stage.width, stage.height - 76);
            verify(stage.previewRect.x > 0);
            verify(stage.previewRect.y > 0);
            verify(stage.previewRect.x + stage.previewRect.width < stage.width);
            verify(stage.previewRect.y + stage.previewRect.height < stage.height - 80);
            // A window covering the whole canvas fills the whole preview.
            const area = stage.workArea, canvas = stage.canvas;
            const frame = stage.windowRect({
                x: (canvas.x - area.x) / area.width,
                y: (canvas.y - area.y) / area.height,
                w: canvas.width / area.width,
                h: canvas.height / area.height
            });
            fuzzyCompare(frame.x, stage.previewRect.x, 0.001);
            fuzzyCompare(frame.y, stage.previewRect.y, 0.001);
            fuzzyCompare(frame.width, stage.previewRect.width, 0.001);
            fuzzyCompare(frame.height, stage.previewRect.height, 0.001);
        }
    }
}
