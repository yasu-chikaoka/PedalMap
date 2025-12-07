import { useRef, useEffect, useState, memo } from 'react';
import { useMapsLibrary } from '@vis.gl/react-google-maps';

interface PlaceAutocompleteProps {
  onPlaceSelect: (place: google.maps.places.PlaceResult) => void;
  placeholder?: string;
  value?: string;
  onChange?: (value: string) => void;
}

export const PlaceAutocomplete = memo(({ onPlaceSelect, placeholder, value, onChange }: PlaceAutocompleteProps) => {
  const [placeAutocomplete, setPlaceAutocomplete] = useState<google.maps.places.Autocomplete | null>(null);
  const inputRef = useRef<HTMLInputElement>(null);
  const places = useMapsLibrary('places');

  useEffect(() => {
    if (!places || !inputRef.current) return;

    const options = {
      fields: ['geometry', 'name', 'formatted_address', 'place_id'],
      componentRestrictions: { country: 'jp' },
    };

    const autocomplete = new places.Autocomplete(inputRef.current, options);
    setPlaceAutocomplete(autocomplete);

    const listener = autocomplete.addListener('place_changed', () => {
      const place = autocomplete.getPlace();
      console.log('PlaceAutocomplete selected:', place);
      
      if (!place.geometry || !place.geometry.location) {
        console.warn('PlaceAutocomplete: No geometry available for selected place');
        return;
      }
      
      onPlaceSelect(place);
    });

    return () => {
        google.maps.event.clearInstanceListeners(autocomplete);
    };
  }, [places, onPlaceSelect]);

  // 外部からのvalue変更を反映させる
  useEffect(() => {
    if (inputRef.current && value !== undefined && inputRef.current.value !== value) {
        inputRef.current.value = value;
    }
  }, [value]);

  return (
    <input
      ref={inputRef}
      className="w-full p-2 border rounded text-sm text-gray-900 placeholder-gray-500"
      placeholder={placeholder || '場所を検索'}
      defaultValue={value}
      onChange={(e) => onChange?.(e.target.value)}
    />
  );
});

PlaceAutocomplete.displayName = 'PlaceAutocomplete';