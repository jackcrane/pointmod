import React from "react";
import "./index.css";
import Dither from "./dither/index.jsx";

export default () => (
  <div className="dither-wrapper">
    <Dither
      waveColor={[0.5, 0.5, 0.5]}
      disableAnimation={false}
      colorNum={4}
      waveAmplitude={0.3}
      waveFrequency={3}
      waveSpeed={0.05}
    />

    <div className="dither-overlay">
      <div className="header">asdf</div>
      <div style={{ flex: 1 }} />
      <div className="titleblock">
        <h1 className="title">Powerful Point Cloud handling</h1>
        <p>Some subtitle text</p>
      </div>
      <div style={{ flex: 1 }} />
    </div>
  </div>
);
