import React, { useEffect, useState } from "react";
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

const ACCESS_FORM_ACTION =
  "https://formkit.xyz/?email=pointmod@cranedigitalplatforms.com";

function AccessButton({ onClick, style }) {
  return (
    <button type="button" className="cta" onClick={onClick} style={style}>
      <span style={{ marginRight: 10 }}>Get Access</span>
      <div className="arrow-line">
        <span>_</span>
      </div>
      <ChevronRight />
    </button>
  );
}

function AccessModal({ isOpen, onClose }) {
  const [submissionState, setSubmissionState] = useState("idle");
  const [errorMessage, setErrorMessage] = useState("");

  useEffect(() => {
    if (isOpen) {
      setSubmissionState("idle");
      setErrorMessage("");
    }
  }, [isOpen]);

  if (!isOpen) {
    return null;
  }

  const handleSubmit = async (event) => {
    event.preventDefault();

    const form = event.currentTarget;
    setSubmissionState("submitting");
    setErrorMessage("");

    try {
      const response = await fetch(ACCESS_FORM_ACTION, {
        method: "POST",
        body: new FormData(form),
        headers: {
          Accept: "application/json",
        },
      });

      if (response.ok) {
        form.reset();
        setSubmissionState("success");
        return;
      }

      const data = await response.json().catch(() => null);

      if (data?.errors?.length) {
        setErrorMessage(data.errors.map((error) => error.message).join(", "));
      } else {
        setErrorMessage("There was a problem submitting the form.");
      }

      setSubmissionState("error");
    } catch (error) {
      setErrorMessage("There was a problem submitting the form.");
      setSubmissionState("error");
    }
  };

  return (
    <div className="modal-backdrop" onClick={onClose}>
      <div
        className="access-modal"
        role="dialog"
        aria-modal="true"
        aria-labelledby="access-modal-title"
        onClick={(event) => event.stopPropagation()}
      >
        <div className="modal-header">
          <div>
            <h2 id="access-modal-title">Request access</h2>
            <p className="modal-copy">
              Tell us who you are and how you plan to use pointmod.
            </p>
          </div>
          <button
            type="button"
            className="modal-close"
            onClick={onClose}
            aria-label="Close access form"
          >
            x
          </button>
        </div>
        {submissionState === "success" ? (
          <div className="modal-success" role="status">
            <p>Thanks for reaching out. We will be in touch soon</p>
          </div>
        ) : (
          <form
            className="access-form"
            method="POST"
            action={ACCESS_FORM_ACTION}
            onSubmit={handleSubmit}
          >
            <label className="field">
              <span>Name</span>
              <input type="text" name="name" autoComplete="name" required />
            </label>
            <label className="field">
              <span>Email</span>
              <input type="email" name="email" autoComplete="email" required />
            </label>
            <div className="field radio-field">
              <span id="use-case-label">Use case</span>
              <div
                className="radio-group"
                role="radiogroup"
                aria-labelledby="use-case-label"
              >
                <label className="radio-option">
                  <input
                    type="radio"
                    name="use_case"
                    value="hobby"
                    defaultChecked
                  />
                  <span className="radio-mark" aria-hidden="true" />
                  <span>Hobby</span>
                </label>
                <label className="radio-option">
                  <input type="radio" name="use_case" value="enterprise" />
                  <span className="radio-mark" aria-hidden="true" />
                  <span>Enterprise</span>
                </label>
                <label className="radio-option">
                  <input type="radio" name="use_case" value="education" />
                  <span className="radio-mark" aria-hidden="true" />
                  <span>Education</span>
                </label>
              </div>
            </div>
            {errorMessage ? (
              <p className="form-status form-status-error" role="alert">
                {errorMessage}
              </p>
            ) : null}
            <button
              type="submit"
              className="cta modal-submit"
              disabled={submissionState === "submitting"}
            >
              <span style={{ marginRight: 10 }}>
                {submissionState === "submitting" ? "Submitting" : "Submit"}
              </span>
              <div className="arrow-line">
                <span>_</span>
              </div>
              <ChevronRight />
            </button>
          </form>
        )}
      </div>
    </div>
  );
}

export default function App() {
  const [isAccessModalOpen, setIsAccessModalOpen] = useState(false);

  useEffect(() => {
    if (!isAccessModalOpen) {
      return undefined;
    }

    const previousOverflow = document.body.style.overflow;
    const handleKeyDown = (event) => {
      if (event.key === "Escape") {
        setIsAccessModalOpen(false);
      }
    };

    document.body.style.overflow = "hidden";
    window.addEventListener("keydown", handleKeyDown);

    return () => {
      document.body.style.overflow = previousOverflow;
      window.removeEventListener("keydown", handleKeyDown);
    };
  }, [isAccessModalOpen]);

  const openAccessModal = () => setIsAccessModalOpen(true);
  const closeAccessModal = () => setIsAccessModalOpen(false);

  return (
    <>
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
              <img
                src={icon}
                className="icon"
                alt="pointmod"
                width="75"
                height="24"
                decoding="async"
              />
              <div style={{ flex: 1 }} />
              <AccessButton onClick={openAccessModal} />
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
              <img
                src={color}
                alt="Color rendering"
                width="1852"
                height="1358"
                loading="lazy"
                decoding="async"
              />
              <div className="caption">
                <h2>Color rendering</h2>
                <p>
                  Get a preview of colored pointclouds with its full embedded
                  colors.
                </p>
              </div>
            </div>
            <div className="item">
              <img
                src={depth}
                alt="Depth rendering"
                width="1852"
                height="1358"
                loading="lazy"
                decoding="async"
              />
              <div className="caption">
                <h2>Depth rendering</h2>
                <p>
                  Make errant points obvious by coloring points by their
                  distance to the camera.
                </p>
              </div>
            </div>
            <div className="item">
              <img
                src={isolation}
                alt="Select isolated points"
                width="1852"
                height="1358"
                loading="lazy"
                decoding="async"
              />
              <div className="caption">
                <h2>Isolated selection</h2>
                <p>
                  Automatically query for points far from their neighbors to
                  easily delete noise.
                </p>
              </div>
            </div>
            <div className="item">
              <img
                src={segmentation}
                alt="Easy segmentation"
                width="1852"
                height="1358"
                loading="lazy"
                decoding="async"
              />
              <div className="caption">
                <h2>Easy segmentation</h2>
                <p>
                  Easily select and delete regions with mouse or geometric
                  selection tools.
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
                  Adjust the size of rendered points to better understand your
                  data.
                </p>
              </div>
            </div>
            <div className="item">
              <img
                src={perf}
                alt="Great performance"
                width="1852"
                height="1358"
                loading="lazy"
                decoding="async"
              />
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
                pointmod. Import huge files, identify and delete noise,
                surgically remove regions and collections of points, and export
                your cleaned file. All done quickly, locally, and in a simple
                UI.
              </div>
            </div>
            <div className="info">
              <div className="title">Inspect</div>
              <div className="detail">
                Dive deep into your pointclouds with the powerful graphic
                inspector. See your pointcloud in full-color 3D, fully
                interactive and explorable. Easily identify outliers, noisy
                surfaces, and unwelcome artifacts using the depth rendering
                mode.
              </div>
            </div>
          </div>
          <div className="footer-cta">
            <h2>
              It's time to move from raw sensor data to clean, usable
              pointclouds.
            </h2>
            <AccessButton
              onClick={openAccessModal}
              style={{
                marginTop: 24,
              }}
            />
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
                <img
                  src={logoFooter}
                  className="icon"
                  alt="pointmod"
                  loading="lazy"
                  decoding="async"
                />
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
                    &copy; {new Date().getFullYear()} Crane Digital Platforms
                    LLC & pointmod
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
      <AccessModal isOpen={isAccessModalOpen} onClose={closeAccessModal} />
    </>
  );
}
