/*
 * Copyright (C) 2016 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

class MacOSMediaControls extends MediaControls
{

    constructor({ width, height, layoutTraits = LayoutTraits.macOS } = {})
    {
        super({ width, height, layoutTraits });

        this.element.classList.add("mac");

        this.muteButton = new MuteButton(this);
        this.tracksButton = new TracksButton(this);
        this.tracksPanel = new TracksPanel;
        this.volumeSlider = new VolumeSlider;

        this.element.addEventListener("mousedown", this);
        this.element.addEventListener("click", this);
    }

    // Public

    showTracksPanel()
    {
        this.tracksButton.on = true;
        this.tracksButton.element.blur();
        this.controlsBar.userInteractionEnabled = false;
        this.controlsBar.hasSecondaryUIAttached = true;
        this.tracksPanel.presentInParent(this);

        const controlsBounds = this.element.getBoundingClientRect();
        const controlsBarBounds = this.controlsBar.element.getBoundingClientRect();
        const tracksButtonBounds = this.tracksButton.element.getBoundingClientRect();
        this.tracksPanel.rightX = this.width - (tracksButtonBounds.right - controlsBounds.left);
        this.tracksPanel.bottomY = this.height - (controlsBarBounds.top - controlsBounds.top) + 1;
        this.tracksPanel.maxHeight = this.height - this.tracksPanel.bottomY - 10;
    }

    hideTracksPanel()
    {
        let shouldFadeControlsBar = true;
        if (window.event instanceof MouseEvent) {
            const x = window.event.clientX;
            const y = window.event.clientY;
            const bounds = this.controlsBar.element.getBoundingClientRect();
            shouldFadeControlsBar = x < bounds.left || x > bounds.right || y < bounds.top || y > bounds.bottom;
        }
            
        this.tracksButton.on = false;
        this.tracksButton.element.focus();
        this.controlsBar.userInteractionEnabled = true;
        this.controlsBar.hasSecondaryUIAttached = false;
        this.controlsBar.faded = shouldFadeControlsBar;
        this.tracksPanel.hide();
    }

    // Protected

    handleEvent(event)
    {
        if (event.currentTarget !== this.element)
            return;

        // Only notify that the background was clicked when the "mousedown" event
        // was also received, which wouldn't happen if the "mousedown" event caused
        // the tracks panel to be hidden, unless we're in fullscreen in which case
        // we can simply check that the panel is not currently presented.
        if (event.type === "mousedown" && !this.tracksPanel.presented)
            this._receivedMousedown = true;
        else if (event.type === "click") {
            if (this._receivedMousedown && event.target === this.element && this.delegate && typeof this.delegate.macOSControlsBackgroundWasClicked === "function")
                this.delegate.macOSControlsBackgroundWasClicked();
            delete this._receivedMousedown
        }
    }

}
