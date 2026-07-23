--TEST--
associative key order vector 035
--FILE--
<?php
$items = ['left' => 45, 4 => 125, 'right' => 205];
unset($items[4]);
$items['tail'] = 206;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:45,205,206:1
