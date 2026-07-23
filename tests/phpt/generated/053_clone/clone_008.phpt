--TEST--
object clone and property COW vector 008
--FILE--
<?php
class GeneratedClone008 { public $items; public function __construct($items) { $this->items = $items; } }
$original = new GeneratedClone008([11, 21]);
$copy = clone $original;
$copy->items[0] = 80;
echo implode(',', $original->items), ':', implode(',', $copy->items);
--EXPECT--
11,21:80,21
