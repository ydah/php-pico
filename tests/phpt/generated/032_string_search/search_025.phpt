--TEST--
string search and slice vector 025
--FILE--
<?php
$text = 'ffneedle25pp';
echo strpos($text, 'needle25'), ':', (str_contains($text, 'needle25') ? 1 : 0), ':', (str_starts_with($text, 'ff') ? 1 : 0), ':', (str_ends_with($text, 'pp') ? 1 : 0), ':', substr($text, 2, 8);
--EXPECT--
2:1:1:1:needle25
