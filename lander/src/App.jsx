import React from "react";
import "./index.css";
import Dither from "./dither/index.jsx";
import icon from "./logo.svg";
import logoFooter from "./logo-footer.svg";
import { ChevronRight } from "./ChevronRight.jsx";
import { Reveal } from "./reveal/reveal.jsx";
import smallPoints from "./screenshots/small-points.png";
import largePoints from "./screenshots/big-points.png";
import color from "./screenshots/color.png";
import depth from "./screenshots/depth.png";
import isolation from "./screenshots/isolation.png";
import perf from "./screenshots/perf.png";
import segmentation from "./screenshots/segmentation.png";

export default () => (
  <main>
    <div className="dither-wrapper header-dither">
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
    <div className="content">
      <div className="grid">
        <div className="item">
          <img src={color} alt="Color rendering" />
          <div className="caption">
            <h2>Color rendering</h2>
            <p>
              Get a preview of colored pointclouds with its full embedded
              colors.
            </p>
          </div>
        </div>
        <div className="item">
          <img src={depth} alt="Depth rendering" />
          <div className="caption">
            <h2>Depth rendering</h2>
            <p>
              Make errant points obvious by coloring points by their distance to
              the camera.
            </p>
          </div>
        </div>
        <div className="item">
          <img src={isolation} alt="Select isolated points" />
          <div className="caption">
            <h2>Isolated selection</h2>
            <p>
              Automatically query for points far from their neighbors to easily
              delete noise.
            </p>
          </div>
        </div>
        <div className="item">
          <img src={segmentation} alt="Easy segmentation" />
          <div className="caption">
            <h2>Easy segmentation</h2>
            <p>
              Easily select and delete regions with mouse or geometric selection
              tools.
            </p>
          </div>
        </div>
        <div className="item">
          <Reveal
            beforeImage={largePoints}
            afterImage={smallPoints}
            beforeAlt="Small points"
            afterAlt="Large points"
          />
          <div className="caption">
            <h2>Customizable points</h2>
            <p>
              Adjust the size of rendered points to better understand your data.
            </p>
          </div>
        </div>
        <div className="item">
          <img src={perf} alt="Great performance" />
          <div className="caption">
            <h2>Great performance</h2>
            <p>
              Optimized rendering and smart tools make rendering and editing
              fast.
            </p>
          </div>
        </div>
      </div>
      <div style={{ marginTop: 64 }}>
        <div className="info">
          <div className="title">Cleanup</div>
          <div className="detail">
            Post processing pointcloud files has never been easier with
            pointmod. Import huge files, identify and delete noise, surgically
            remove regions and collections of points, and export your cleaned
            file. All done quickly, locally, and in a simple UI.
          </div>
        </div>
        <div className="info">
          <div className="title">Inspect</div>
          <div className="detail">
            Dive deep into your pointclouds with the powerful graphic inspector.
            See your pointcloud in full-color 3D, fully interactive and
            explorable. Easily identify outliers, noisy surfaces, and unwelcome
            artifacts using the depth rendering mode.
          </div>
        </div>
      </div>
      <div className="footer-cta">
        <h2>
          It's time to move from raw sensor data to clean, usable pointclouds.
        </h2>
        <a
          href=""
          className="cta"
          style={{
            marginTop: 24,
          }}
        >
          <span style={{ marginRight: 10 }}>Get Access</span>
          <div className="arrow-line">
            <span>_</span>
          </div>
          <ChevronRight />
        </a>
      </div>
    </div>
    <footer className="footer">
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
          <div className="footer-content">
            <img src={logoFooter} className="icon" alt="icon" />
            <div
              style={{
                display: "flex",
                flexDirection: "column",
                alignItems: "flex-end",
                flex: 1,
                height: "80%",
                gap: 8,
              }}
            >
              <span>
                A product of{" "}
                <a href="https://cranedigitalplatforms.com/">
                  Crane Digital Platforms
                </a>
              </span>
              <span>
                {/* copyright */}
                &copy; {new Date().getFullYear()} Crane Digital Platforms LLC &
                pointmod
              </span>
              <span>pointmod does not collect or share any data</span>
              <div style={{ flex: 1 }} />
              <a href="mailto:pointmod@cranedigitalplatforms.com">
                pointmod@cranedigitalplatforms.com
              </a>
            </div>
          </div>
        </div>
      </div>
    </footer>
  </main>
);
