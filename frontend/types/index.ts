// API Response Types
export interface RouteSummary {
  total_distance_m: number;
  total_elevation_gain_m: number;
  estimated_moving_time_s: number;
}

export interface Stop {
  name: string;
  type: string;
  rating: number;
  location: { lat: number; lon: number };
}

export interface RouteResponse {
  summary: RouteSummary;
  geometry: string; // Encoded Polyline
  stops?: Stop[];
}

// UI State Types
export interface Waypoint {
  id: string;
  name: string;
  location: { lat: number; lng: number };
}

export interface Location {
  lat: number;
  lng: number;
}
