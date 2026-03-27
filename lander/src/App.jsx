import React from "react";
import "./index.css";
import Dither from "./dither/index.jsx";
import icon from "./logo.svg";
import { ChevronRight } from "./ChevronRight.jsx";

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
      <div className="header">
        <img src={icon} className="icon" alt="icon" />
        <div style={{ flex: 1 }} />
        <a href="" className="cta">
          <span style={{ marginRight: 10 }}>Get Access</span>
          <div className="arrow-line">
            <span>_</span>
          </div>
          <ChevronRight />
        </a>
      </div>
      <div style={{ flex: 1 }} />
      <div className="titleblock">
        <h1 className="title">Powerful Point Cloud handling</h1>
      </div>
      <div style={{ flex: 1 }} />
    </div>
  </div>
);
