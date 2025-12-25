import { useState, useCallback } from 'react';
import toast from 'react-hot-toast';
import { APP_CONFIG, UI_TEXT } from '@/config/constants';
import type { RouteResponse, Waypoint } from '@/types';

// Types
interface Coordinates {
  lat: number;
  lng: number;
}

interface UseRouteProps {
  startPoint: Coordinates;
  endPoint: Coordinates;
  waypoints: Waypoint[];
  targetDistance: number;
  targetElevation: number;
}

interface UseRouteReturn {
  routeData: RouteResponse | null;
  loading: boolean;
  error: string | null;
  generateRoute: () => Promise<void>;
  resetRoute: () => void;
}

export const useRoute = ({
  startPoint,
  endPoint,
  waypoints,
  targetDistance,
  targetElevation,
}: UseRouteProps): UseRouteReturn => {
  const [routeData, setRouteData] = useState<RouteResponse | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const resetRoute = useCallback(() => {
    setRouteData(null);
    setError(null);
  }, []);

  const generateRoute = useCallback(async () => {
    setLoading(true);
    setError(null);
    setRouteData(null);

    const toastId = toast.loading(UI_TEXT.BUTTONS.SEARCHING);

    try {
      const response = await fetch(
        `${APP_CONFIG.API.BASE_URL}${APP_CONFIG.API.ENDPOINTS.GENERATE_ROUTE}`,
        {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: JSON.stringify({
            start_point: { lat: startPoint.lat, lon: startPoint.lng },
            end_point: { lat: endPoint.lat, lon: endPoint.lng },
            waypoints: waypoints.map((wp) => ({
              lat: wp.location.lat,
              lon: wp.location.lng,
            })),
            preferences: {
              target_distance_km: targetDistance,
              target_elevation_gain_m: targetElevation,
            },
          }),
        },
      );

      if (!response.ok) {
        throw new Error(UI_TEXT.MESSAGES.ROUTE_SEARCH_ERROR);
      }

      const data = await response.json();
      setRouteData(data);
      toast.success(UI_TEXT.TITLES.ROUTE_RESULT, { id: toastId });
    } catch (err) {
      const errorMessage =
        err instanceof Error
          ? err.message
          : UI_TEXT.MESSAGES.UNEXPECTED_ERROR;
      setError(errorMessage);
      toast.error(errorMessage, { id: toastId });
    } finally {
      setLoading(false);
    }
  }, [startPoint, endPoint, waypoints, targetDistance, targetElevation]);

  return {
    routeData,
    loading,
    error,
    generateRoute,
    resetRoute,
  };
};
