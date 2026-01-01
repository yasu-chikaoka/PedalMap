import { useRef, useEffect, memo } from 'react';
import { useMapsLibrary } from '@vis.gl/react-google-maps';

interface PlaceAutocompleteProps {
  onPlaceSelect: (
    place: google.maps.places.PlaceResult,
    inputValue: string,
  ) => void;
  placeholder?: string;
  value?: string;
}

export const PlaceAutocomplete = memo(
  ({ onPlaceSelect, placeholder, value }: PlaceAutocompleteProps) => {
    const inputRef = useRef<HTMLInputElement>(null);
    const places = useMapsLibrary('places');

    useEffect(() => {
      if (!places || !inputRef.current) return;

      const options = {
        fields: ['geometry', 'name', 'formatted_address', 'place_id'],
        componentRestrictions: { country: 'jp' },
      };

      const autocomplete = new places.Autocomplete(inputRef.current, options);

      const handlePlaceChanged = () => {
        const place = autocomplete.getPlace();

        if (!place.geometry || !place.geometry.location) {
          console.warn(
            'PlaceAutocomplete: No geometry available for selected place',
          );
          return;
        }

        // inputRef.current.value には、Autocompleteによって選択された名称が入っている
        onPlaceSelect(place, inputRef.current?.value || '');
      };

      autocomplete.addListener('place_changed', handlePlaceChanged);

      return () => {
        google.maps.event.clearInstanceListeners(autocomplete);
      };
    }, [places, onPlaceSelect]);

    // 外部からのvalue変更（現在地取得など）を反映させる
    // ユーザーが入力をタイプしている最中は、外部のvalueも変化しない（ControlPanelでonChangeを外したため）
    useEffect(() => {
      if (
        inputRef.current &&
        value !== undefined &&
        inputRef.current.value !== value
      ) {
        inputRef.current.value = value;
      }
    }, [value]);

    return (
      <input
        ref={inputRef}
        className="w-full p-2.5 bg-white border border-gray-300 rounded-lg text-sm focus:ring-2 focus:ring-blue-500 focus:border-blue-500 transition-all shadow-sm text-gray-900 placeholder-gray-400"
        placeholder={placeholder || '場所を検索'}
      />
    );
  },
);

PlaceAutocomplete.displayName = 'PlaceAutocomplete';
