# frozen_string_literal: true

# SketchUp Ruby Script
# File: corexy_balanced_routing_demo.rb
# Purpose:
#   Build a top-view routing diagram for a symmetric, CoreXY-like belt layout
#   to reduce carriage eccentric force (racking).
#
# Usage:
#   1) Open SketchUp
#   2) Ruby Console -> load '/Users/convel/Documents/mTabula/cad/skp/corexy_balanced_routing_demo.rb'
#   3) Run: CoreXYBalancedRoutingDemo.build

module CoreXYBalancedRoutingDemo
  extend self

  GREEN = Sketchup::Color.new(40, 190, 80)
  RED   = Sketchup::Color.new(210, 45, 45)
  BLUE  = Sketchup::Color.new(45, 95, 220)
  ORANGE = Sketchup::Color.new(230, 140, 40)
  GRAY  = Sketchup::Color.new(190, 190, 190)
  BLACK = Sketchup::Color.new(30, 30, 30)

  def mm(v)
    v.to_f.mm
  end

  def pt(x, y, z = 0)
    Geom::Point3d.new(mm(x), mm(y), mm(z))
  end

  def add_polyline(ents, points, close: false)
    return if points.length < 2
    (0...(points.length - 1)).each do |i|
      ents.add_line(points[i], points[i + 1])
    end
    ents.add_line(points[-1], points[0]) if close
  end

  def add_circle_marker(ents, center, r_mm, color, face: false)
    edges = ents.add_circle(center, Z_AXIS, mm(r_mm), 28)
    if face
      f = ents.add_face(edges)
      f.material = color
      f.back_material = color
    end
    edges.each { |e| e.material = color }
  end

  def add_filled_rect(ents, x1, y1, x2, y2, color)
    p1 = pt(x1, y1)
    p2 = pt(x2, y1)
    p3 = pt(x2, y2)
    p4 = pt(x1, y2)
    f = ents.add_face(p1, p2, p3, p4)
    return unless f
    f.material = color
    f.back_material = color
    f.edges.each { |e| e.material = BLACK }
    f
  end

  def add_text(ents, text, x, y)
    ents.add_text(text, pt(x, y, 0), pt(x + 2, y + 2, 0))
  end

  def build
    model = Sketchup.active_model
    model.start_operation('CoreXY Balanced Routing Demo', true)

    ents = model.active_entities

    # Build in a new root group to avoid touching existing geometry.
    root = ents.add_group
    g = root.entities

    # ---- Frame ----
    add_filled_rect(g, -220, -220, 220, 220, GRAY)

    # Cut inner window (diagram look only).
    inner = add_filled_rect(g, -200, -200, 200, 200, Sketchup::Color.new(245, 245, 245))
    inner.erase! if inner

    frame_edges = [
      [pt(-200, -200), pt(200, -200)],
      [pt(200, -200), pt(200, 200)],
      [pt(200, 200), pt(-200, 200)],
      [pt(-200, 200), pt(-200, -200)]
    ]
    frame_edges.each { |a, b| e = g.add_line(a, b); e.material = BLACK }

    # ---- Orthogonal guide rails / cross ----
    add_filled_rect(g, -185, -8, 185, 8, ORANGE)   # X beam
    add_filled_rect(g, -8, -185, 8, 185, ORANGE)   # Y beam

    # ---- Carriage block ----
    add_filled_rect(g, -28, -28, 28, 28, Sketchup::Color.new(155, 155, 155))

    # ---- Motors (fixed on frame) ----
    add_filled_rect(g, -150, -205, -95, -165, Sketchup::Color.new(205, 145, 145))
    add_filled_rect(g, 95, -205, 150, -165, Sketchup::Color.new(205, 145, 145))

    add_text(g, 'Motor A', -148, -210)
    add_text(g, 'Motor B', 98, -210)

    # ---- Idlers (green markers) ----
    idlers = {
      lt: [-120, 170],
      rt: [120, 170],
      lb: [-120, -170],
      rb: [120, -170],
      ct1: [-18, 20],
      ct2: [18, 20],
      cb1: [-18, -20],
      cb2: [18, -20],
      xL: [-190, 0],
      xR: [190, 0]
    }

    idlers.each_value do |x, y|
      add_circle_marker(g, pt(x, y), 8, GREEN, face: true)
    end

    # ---- Belt A (red) ----
    belt_a = [
      pt(-122, -170),
      pt(-122, 170),
      pt(-18, 20),
      pt(-28, 0),
      pt(18, -20),
      pt(122, -170),
      pt(122, 170),
      pt(28, 0)
    ]
    add_polyline(g, belt_a, close: true)
    g.grep(Sketchup::Edge).last(belt_a.length).each { |e| e.material = RED }

    # ---- Belt B (blue) ----
    belt_b = [
      pt(122, -170),
      pt(122, 170),
      pt(18, 20),
      pt(28, 0),
      pt(-18, -20),
      pt(-122, -170),
      pt(-122, 170),
      pt(-28, 0)
    ]
    add_polyline(g, belt_b, close: true)
    g.grep(Sketchup::Edge).last(belt_b.length).each { |e| e.material = BLUE }

    # ---- Anchors on carriage ----
    add_circle_marker(g, pt(-28, 0), 2.8, BLACK, face: true)
    add_circle_marker(g, pt(28, 0), 2.8, BLACK, face: true)

    # ---- Labels ----
    add_text(g, 'Symmetric CoreXY-like routing', -196, 214)
    add_text(g, 'Balanced dual-belt forces at carriage', -196, 202)
    add_text(g, 'Red=Belt A, Blue=Belt B, Green=Idlers', -196, 190)

    model.commit_operation
    puts '[CoreXYBalancedRoutingDemo] Done.'
  rescue StandardError => e
    model.abort_operation
    puts "[CoreXYBalancedRoutingDemo] Error: #{e.message}"
    puts e.backtrace
  end
end
