mu = {}
mu.NOTE_NAMES = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}
mu.SCALES = {
  {name = "Major", alt_names = {"Ionian"}, intervals = {0, 2, 4, 5, 7, 9, 11, 12}, },
  {name = "Natural Minor", alt_names = {"Minor", "Aeolian"}, intervals = {0, 2, 3, 5, 7, 8, 10, 12}, },
  {name = "Harmonic Minor", intervals = {0, 2, 3, 5, 7, 8, 11, 12}, },
  {name = "Melodic Minor", intervals = {0, 2, 3, 5, 7, 9, 11, 12}, },
  {name = "Dorian", intervals = {0, 2, 3, 5, 7, 9, 10, 12}, },
  {name = "Phrygian", intervals = {0, 1, 3, 5, 7, 8, 10, 12}, },
  {name = "Lydian", intervals = {0, 2, 4, 6, 7, 9, 11, 12}, },
  {name = "Mixolydian", intervals = {0, 2, 4, 5, 7, 9, 10, 12}, },
  {name = "Locrian", intervals = {0, 1, 3, 5, 6, 8, 10, 12}, },
  {name = "Whole Tone", intervals = {0, 2, 4, 6, 8, 10, 12}, },
  {name = "Major Pentatonic", alt_names = {"Gagaku Ryo Sen Pou"}, intervals = {0, 2, 4, 7, 9, 12}, },
  {name = "Minor Pentatonic", alt_names = {"Zokugaku Yo Sen Pou"}, intervals = {0, 3, 5, 7, 10, 12}, },
  {name = "Major Bebop", intervals = {0, 2, 4, 5, 7, 8, 9, 11, 12}, },
  {name = "Altered Scale", intervals = {0, 1, 3, 4, 6, 8, 10, 12}, },
  {name = "Dorian Bebop", intervals = {0, 2, 3, 4, 5, 7, 9, 10, 12}, },
  {name = "Mixolydian Bebop", intervals = {0, 2, 4, 5, 7, 9, 10, 11, 12}, },
  {name = "Blues Scale", alt_names = {"Blues"}, intervals = {0, 3, 5, 6, 7, 10, 12}, },
  {name = "Diminished Whole Half", intervals = {0, 2, 3, 5, 6, 8, 9, 11, 12}, },
  {name = "Diminished Half Whole", intervals = {0, 1, 3, 4, 6, 7, 9, 10, 12}, },
  {name = "Neapolitan Major", intervals = {0, 1, 3, 5, 7, 9, 11, 12}, },
  {name = "Hungarian Major", intervals = {0, 3, 4, 6, 7, 9, 10, 12}, },
  {name = "Harmonic Major", intervals = {0, 2, 4, 5, 7, 8, 11, 12}, },
  {name = "Hungarian Minor", intervals = {0, 2, 3, 6, 7, 8, 11, 12}, },
  {name = "Lydian Minor", intervals = {0, 2, 4, 6, 7, 8, 10, 12}, },
  {name = "Neapolitan Minor", alt_names = {"Byzantine"}, intervals = {0, 1, 3, 5, 7, 8, 11, 12}, },
  {name = "Major Locrian", intervals = {0, 2, 4, 5, 6, 8, 10, 12}, },
  {name = "Leading Whole Tone", intervals = {0, 2, 4, 6, 8, 10, 11, 12}, },
  {name = "Six Tone Symmetrical", intervals = {0, 1, 4, 5, 8, 9, 11, 12}, },
  {name = "Balinese", intervals = {0, 1, 3, 7, 8, 12}, },
  {name = "Persian", intervals = {0, 1, 4, 5, 6, 8, 11, 12}, },
  {name = "East Indian Purvi", intervals = {0, 1, 4, 6, 7, 8, 11, 12}, },
  {name = "Oriental", intervals = {0, 1, 4, 5, 6, 9, 10, 12}, },
  {name = "Double Harmonic", intervals = {0, 1, 4, 5, 7, 8, 11, 12}, },
  {name = "Enigmatic", intervals = {0, 1, 4, 6, 8, 10, 11, 12}, },
  {name = "Overtone", intervals = {0, 2, 4, 6, 7, 9, 10, 12}, },
  {name = "Eight Tone Spanish", intervals = {0, 1, 3, 4, 5, 6, 8, 10, 12}, },
  {name = "Prometheus", intervals = {0, 2, 4, 6, 9, 10, 12}, },
  {name = "Gagaku Rittsu Sen Pou", intervals = {0, 2, 5, 7, 9, 10, 12}, },
  {name = "In Sen Pou", intervals = {0, 1, 5, 2, 8, 12}, },
  {name = "Okinawa", intervals = {0, 4, 5, 7, 11, 12}, },
  {name = "Chromatic", intervals = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}, },
}
mu.CHORDS = {
  {name = "Major", alt_names = {"Maj"}, intervals = {0, 4, 7}},
  {name = "Major 6", alt_names = {"Maj6"}, intervals = {0, 4, 7, 9}},
  {name = "Major 7", alt_names = {"Maj7"}, intervals = {0, 4, 7, 11}},
  {name = "Major 69", alt_names = {"Maj69"}, intervals = {0, 4, 7, 9, 14}},
  {name = "Major 9", alt_names = {"Maj9"}, intervals = {0, 4, 7, 11, 14}},
  {name = "Major 11", alt_names = {"Maj11"}, intervals = {0, 4, 7, 11, 14, 17}},
  {name = "Major 13", alt_names = {"Maj13"}, intervals = {0, 4, 7, 11, 14, 17, 21}},
  {name = "Dominant 7", intervals = {0, 4, 7, 10}},
  {name = "Ninth", intervals = {0, 4, 7, 10, 14}},
  {name = "Eleventh", intervals = {0, 4, 7, 10, 14, 17}},
  {name = "Thirteenth", intervals = {0, 4, 7, 10, 14, 17, 21}},
  {name = "Augmented", intervals = {0, 4, 8}},
  {name = "Augmented 7", intervals = {0, 4, 8, 10}},
  {name = "Sus4", intervals = {0, 5, 7}},
  {name = "Seventh sus4", intervals = {0, 5, 7, 10}},
  {name = "Minor Major 7", alt_names = {"MinMaj7"}, intervals = {0, 3, 7, 11}},
  {name = "Minor", alt_names = {"Min"}, intervals = {0, 3, 7}},
  {name = "Minor 6", alt_names = {"Min6"}, intervals = {0, 3, 7, 9}},
  {name = "Minor 7", alt_names = {"Min7"}, intervals = {0, 3, 7, 10}},
  {name = "Minor 69", alt_names = {"Min69"}, intervals = {0, 3, 7, 9, 14}},
  {name = "Minor 9", alt_names = {"Min9"}, intervals = {0, 3, 7, 10, 14}},
  {name = "Minor 11", alt_names = {"Min11"}, intervals = {0, 3, 7, 10, 14, 17}},
  {name = "Minor 13", alt_names = {"Min13"}, intervals = {0, 3, 7, 10, 14, 17, 21}},
  {name = "Diminished", alt_names = {"Dim"}, intervals = {0, 3, 6}},
  {name = "Diminished 7", alt_names = {"Dim7"}, intervals = {0, 3, 6, 9}},
  {name = "Half Diminished 7", alt_names = {"Min7b5"}, intervals = {0, 3, 6, 10}},
  {name = "Augmented Major 7", alt_names = {"Maj7#5"}, intervals = {0, 4, 8, 11}}
}

function lookup_data(lookup_table, search)
  if type(search) == "string" then
    search = string.lower(search)
    for i = 1, #lookup_table do
      if string.lower(lookup_table[i].name) == search then
        search = i
        break
      elseif lookup_table[i].alt_names then
        local found = false
        for j = 1, #lookup_table[i].alt_names do
          if string.lower(lookup_table[i].alt_names[j]) == search then
            search = i
            found = true
            break
          end
        end
        if found then break end
      end
    end
  end
  return lookup_table[search]
end

function generate_scale_array(root_num, scale_data, length)
  local out_array = {}
  local scale_len = #scale_data.intervals
  local note_num
  local i = 0
  while #out_array < length do
    if i > 0 and i % scale_len == 0 then
      root_num = root_num + scale_data.intervals[scale_len]
    else
      note_num = root_num + scale_data.intervals[i % scale_len + 1]
      if note_num > 127 then break
      else table.insert(out_array, note_num) end
    end
    i = i + 1
  end
  return out_array
end

function mu.generate_scale(root_num, scale_type, octaves)
  if type(root_num) ~= "number" or root_num < 0 or root_num > 127 then return nil end
  scale_type = scale_type or 1
  octaves = octaves or 1
  local scale_data = lookup_data(mu.SCALES, scale_type)
  if not scale_data then return nil end
  local length = octaves * #scale_data.intervals - (round(octaves) - 1)
  return generate_scale_array(root_num, scale_data, length)
end

function mu.generate_scale_of_length(root_num, scale_type, length)
  length = length or 8
  local scale_data = lookup_data(mu.SCALES, scale_type)
  if not scale_data then return nil end
  return generate_scale_array(root_num, scale_data, length)
end

function mu.generate_chord(root_num, chord_type, inversion)
  if type(root_num) ~= "number" or root_num < 0 or root_num > 127 then return nil end
  chord_type = chord_type or 1
  inversion = inversion or 0
  local chord_data = lookup_data(mu.CHORDS, chord_type)
  if not chord_data then return nil end
  local out_array = {}
  for i = 1, #chord_data.intervals do
    local note_num = root_num + chord_data.intervals[i]
    if note_num > 127 then break end
    table.insert(out_array, note_num)
  end
  for i = 1, util.clamp(inversion, 0, #out_array - 1) do
    local head = table.remove(out_array, 1)
    table.insert(out_array, head + 12)
  end
  return out_array
end

function mu.snap_note_to_array(note_num, snap_array)
  local snap_array_len = #snap_array
  if snap_array_len == 1 then
    note_num = snap_array[1]
  elseif note_num >= snap_array[snap_array_len] then
    note_num = snap_array[snap_array_len]
  else
    local delta
    local prev_delta = math.huge
    for s = 1, snap_array_len + 1 do
      if s > snap_array_len then
        note_num = note_num + prev_delta
        break
      end
      delta = snap_array[s] - note_num
      if delta == 0 then
        break
      elseif math.abs(delta) >= math.abs(prev_delta) then
        note_num = note_num + prev_delta
        break
      end
      prev_delta = delta
    end
  end
  return note_num
end

function mu.snap_notes_to_array(note_nums_array, snap_array)
  for i = 1, #note_nums_array do
    note_nums_array[i] = mu.snap_note_to_array(note_nums_array[i], snap_array)
  end
  return note_nums_array
end

function mu.note_num_to_name(note_num, include_octave)
  local name = mu.NOTE_NAMES[note_num % 12 + 1]
  if include_octave then name = name .. math.floor(note_num / 12 - 2) end
  return name
end

function mu.note_nums_to_names(note_nums_array, include_octave)
  local out_array = {}
  for i = 1, #note_nums_array do
    out_array[i] = mu.note_num_to_name(note_nums_array[i], include_octave)
  end
  return out_array
end

function mu.note_num_to_freq(note_num)
  return 13.75 * (2 ^ ((note_num - 9) / 12))
end

function mu.note_nums_to_freqs(note_nums_array)
  local out_array = {}
  for i = 1, #note_nums_array do
    out_array[i] = mu.note_num_to_freq(note_nums_array[i])
  end
  return out_array
end

function mu.freq_to_note_num(freq)
  return util.clamp(math.floor(12 * math.log(freq / 440.0) / math.log(2) + 69.5), 0, 127)
end

function mu.freqs_to_note_nums(freqs_array)
  local out_array = {}
  for i = 1, #freqs_array do
    out_array[i] = mu.freq_to_note_num(freqs_array[i])
  end
  return out_array
end

function mu.interval_to_ratio(interval)
  return math.pow(2, interval / 12)
end

function mu.intervals_to_ratios(intervals_array)
  local out_array = {}
  for i = 1, #intervals_array do
    out_array[i] = mu.interval_to_ratio(intervals_array[i])
  end
  return out_array
end

function mu.ratio_to_interval(ratio)
  return 12 * math.log(ratio) / math.log(2)
end

function mu.ratios_to_intervals(ratios_array)
  local out_array = {}
  for i = 1, #ratios_array do
    out_array[i] = mu.ratio_to_interval(ratios_array[i])
  end
  return out_array
end

print('module: mu')
