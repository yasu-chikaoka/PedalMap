'use client';

import { Clock, MapPin } from 'lucide-react';
import { UI_TEXT } from '@/config/constants';
import type { RouteResponse, Stop } from '@/types';

interface RouteStatsProps {
  routeData: RouteResponse;
  onSpotClick: (stop: Stop) => void;
}

export const RouteStats = ({ routeData, onSpotClick }: RouteStatsProps) => {
  return (
    <div className="p-4 bg-green-50 border border-green-200 rounded-lg space-y-3">
      <h3 className="font-bold text-green-800 border-b border-green-200 pb-2">
        {UI_TEXT.TITLES.ROUTE_RESULT}
      </h3>
      <div className="grid grid-cols-2 gap-4">
        <div>
          <p className="text-xs text-gray-500 uppercase">
            {UI_TEXT.LABELS.TOTAL_DISTANCE}
          </p>
          <p className="text-xl font-mono font-bold text-gray-800">
            {(routeData.summary.total_distance_m / 1000).toFixed(2)}{' '}
            <span className="text-sm font-normal">km</span>
          </p>
        </div>
        <div>
          <p className="text-xs text-gray-500 uppercase">{UI_TEXT.LABELS.EST_TIME}</p>
          <p className="text-xl font-mono font-bold text-gray-800 flex items-center gap-1">
            <Clock size={16} className="text-gray-400" />
            {Math.floor(
              routeData.summary.estimated_moving_time_s / 60,
            )}{' '}
            <span className="text-sm font-normal">min</span>
          </p>
        </div>
        <div>
          <p className="text-xs text-gray-500 uppercase">
            {UI_TEXT.LABELS.ELEVATION_GAIN}
          </p>
          <p className="text-xl font-mono font-bold text-gray-800">
            {routeData.summary.total_elevation_gain_m}{' '}
            <span className="text-sm font-normal">m</span>
          </p>
        </div>
      </div>

      {routeData.stops && routeData.stops.length > 0 ? (
        <div className="mt-4 border-t border-green-200 pt-3">
          <h4 className="font-semibold text-green-800 mb-2 text-sm flex items-center gap-1">
            <MapPin size={14} /> {UI_TEXT.TITLES.RECOMMENDED_SPOTS}
          </h4>
          <ul className="space-y-2">
            {routeData.stops.map((stop, i) => (
              <li
                key={i}
                className="bg-white p-2 rounded border border-gray-100 text-sm shadow-sm cursor-pointer hover:bg-blue-50 transition-colors"
                onClick={() => onSpotClick(stop)}
              >
                <div className="font-bold text-gray-700">{stop.name}</div>
                <div className="flex justify-between text-xs text-gray-500 mt-1">
                  <span className="bg-blue-100 text-blue-800 px-1.5 py-0.5 rounded capitalize">
                    {stop.type}
                  </span>
                  <span className="flex items-center gap-0.5 text-orange-500 font-bold">
                    ★ {stop.rating}
                  </span>
                </div>
              </li>
            ))}
          </ul>
        </div>
      ) : (
        <div className="mt-4 border-t border-green-200 pt-3">
          <h4 className="font-semibold text-green-800 mb-2 text-sm flex items-center gap-1">
            <MapPin size={14} /> {UI_TEXT.TITLES.RECOMMENDED_SPOTS}
          </h4>
          <p className="text-sm text-gray-500 italic">
            {UI_TEXT.MESSAGES.NO_SPOTS_FOUND}
          </p>
        </div>
      )}
    </div>
  );
};
