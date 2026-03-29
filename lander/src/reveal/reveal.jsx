import React, { useRef, useState } from "react";
import "./reveal.css";

export const Reveal = ({
  beforeImage,
  afterImage,
  beforeAlt = "Before image",
  afterAlt = "After image",
  className = "",
}) => {
  const containerRef = useRef(null);
  const [position, setPosition] = useState(50);
  const [isHovered, setIsHovered] = useState(false);

  const updatePosition = (clientX) => {
    const el = containerRef.current;
    if (!el) return;

    const rect = el.getBoundingClientRect();
    const next = ((clientX - rect.left) / rect.width) * 100;
    const clamped = Math.max(0, Math.min(100, next));

    setPosition(clamped);
  };

  const handleMouseMove = (event) => {
    updatePosition(event.clientX);
  };

  const handleTouchMove = (event) => {
    const touch = event.touches?.[0];
    if (!touch) return;
    updatePosition(touch.clientX);
  };

  const handleMouseLeave = () => {
    setIsHovered(false);
    setPosition(50);
  };

  const handleMouseEnter = () => {
    setIsHovered(true);
  };

  return (
    <div
      ref={containerRef}
      className={`picture-reveal ${className}`.trim()}
      onMouseEnter={handleMouseEnter}
      onMouseMove={handleMouseMove}
      onMouseLeave={handleMouseLeave}
      onTouchStart={handleMouseEnter}
      onTouchMove={handleTouchMove}
      onTouchEnd={handleMouseLeave}
      style={{ "--picture-reveal-position": `${position}%` }}
    >
      <img
        src={beforeImage}
        alt={beforeAlt}
        className="picture-reveal__image picture-reveal__image--base"
        width="1852"
        height="1358"
        loading="lazy"
        decoding="async"
        draggable="false"
      />

      <div className="picture-reveal__overlay">
        <img
          src={afterImage}
          alt={afterAlt}
          className="picture-reveal__image picture-reveal__image--overlay"
          width="1852"
          height="1358"
          loading="lazy"
          decoding="async"
          draggable="false"
        />
      </div>

      <div className="picture-reveal__divider" aria-hidden="true"></div>
    </div>
  );
};
