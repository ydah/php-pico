--TEST--
string search and slice vector 005
--FILE--
<?php
$text = 'fffneedle05pp';
echo strpos($text, 'needle05'), ':', (str_contains($text, 'needle05') ? 1 : 0), ':', (str_starts_with($text, 'fff') ? 1 : 0), ':', (str_ends_with($text, 'pp') ? 1 : 0), ':', substr($text, 3, 8);
--EXPECT--
3:1:1:1:needle05
